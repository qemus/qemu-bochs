# syntax=docker/dockerfile:1

FROM ubuntu:26.04 AS toolchain

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      autoconf \
      automake \
      bison \
      build-essential \
      bzip2 \
      ca-certificates \
      cmake \
      curl \
      file \
      flex \
      gawk \
      gettext \
      git \
      libgmp-dev \
      libmpc-dev \
      libmpfr-dev \
      libncurses-dev \
      libtool \
      m4 \
      make \
      ninja-build \
      patch \
      perl \
      pkg-config \
      python3 \
      python-is-python3 \
      rsync \
      subversion \
      sudo \
      tar \
      texinfo \
      unzip \
      wget \
      xz-utils \
      zip \
      zlib1g-dev && \
    rm -rf /var/lib/apt/lists/*

# Use the same RosBE bootstrap used by the upstream ReactOS Linux CI.
# The helper does not reliably propagate download/prerequisite failures, so
# verify the resulting toolchain and retry transient bootstrap failures.
RUN wget -qO /tmp/build_rosbe_ci.sh \
      https://gist.githubusercontent.com/zefklop/b2d6a0b470c70183e93d5285a03f5899/raw/build_rosbe_ci.sh && \
    chmod +x /tmp/build_rosbe_ci.sh && \
    for attempt in 1 2 3; do \
      rm -rf /opt/RosBE /tmp/rosbe-build && \
      mkdir -p /tmp/rosbe-build && \
      (cd /tmp/rosbe-build && /tmp/build_rosbe_ci.sh /opt/RosBE) && \
      test -x /opt/RosBE/RosBE.sh && break; \
      echo "RosBE bootstrap attempt ${attempt} failed; retrying..." >&2; \
      sleep $((attempt * 5)); \
    done && \
    test -x /opt/RosBE/RosBE.sh && \
    rm -rf /tmp/build_rosbe_ci.sh /tmp/rosbe-build

FROM toolchain AS builder

ARG VERSION_ARG="0.0"
ARG REACTOS_COMMIT="c7e1c79921f30275f32ce916b6517a78bcf05368"

WORKDIR /work

RUN git clone --filter=blob:none --no-checkout https://github.com/reactos/reactos.git /reactos && \
    cd /reactos && \
    git fetch --depth=1 origin "${REACTOS_COMMIT}" && \
    git checkout --detach FETCH_HEAD

COPY src/ /qbochs/src/
COPY Dockerfile readme.md license.md /qbochs/

# Replace only the upstream Bochs miniport build directory. The rest of the
# ReactOS tree supplies the NT5 headers, import libraries, and build machinery.
RUN rm -rf /reactos/win32ss/drivers/miniport/bochs/* && \
    cp /qbochs/src/qbochs.c /reactos/win32ss/drivers/miniport/bochs/ && \
    cp /qbochs/src/qbochs.h /reactos/win32ss/drivers/miniport/bochs/ && \
    cp /qbochs/src/qbochs.rc /reactos/win32ss/drivers/miniport/bochs/ && \
    cp /qbochs/src/CMakeLists.txt /reactos/win32ss/drivers/miniport/bochs/

RUN <<'EOF_BUILD'
set -eux

for arch in i386 amd64; do
  echo "cmake -S /reactos -B /build-${arch} -G Ninja -DCMAKE_TOOLCHAIN_FILE:FILEPATH=toolchain-gcc.cmake -DARCH:STRING=${arch} -DCMAKE_BUILD_TYPE=Release -DDLL_EXPORT_VERSION=0x502 -DENABLE_ROSTESTS=0 -DENABLE_ROSAPPS=0" \
    | /opt/RosBE/RosBE.sh . 0 "${arch}"

  echo "cmake --build /build-${arch} --target qbochs -- -k0" \
    | /opt/RosBE/RosBE.sh . 0 "${arch}"
done

mkdir -p /dist /package-x86 /package-x64

driver_version="${VERSION_ARG}"
case "${driver_version}" in
  *.*.*.*) ;;
  *.*.*) driver_version="${driver_version}.0" ;;
  *.*) driver_version="${driver_version}.0.0" ;;
  *) driver_version="${driver_version}.0.0.0" ;;
esac

x86_sys="$(find /build-i386 -type f -name qbochs.sys -print -quit)"
x64_sys="$(find /build-amd64 -type f -name qbochs.sys -print -quit)"

test -n "${x86_sys}"
test -n "${x64_sys}"

cp "${x86_sys}" /package-x86/qbochs.sys
cp "${x64_sys}" /package-x64/qbochs.sys
cp /qbochs/license.md /package-x86/license.txt
cp /qbochs/license.md /package-x64/license.txt

sed \
  -e "s/@ARCH@/x86/g" \
  -e "s/@VERSION@/${driver_version}/g" \
  /qbochs/src/qbochs.inf.in > /package-x86/qbochs.inf

sed \
  -e "s/@ARCH@/amd64/g" \
  -e "s/@VERSION@/${driver_version}/g" \
  /qbochs/src/qbochs.inf.in > /package-x64/qbochs.inf

(
  cd /package-x86
  zip -9 -q "/dist/QBochs-${VERSION_ARG}-x86.zip" qbochs.sys qbochs.inf license.txt
)

(
  cd /package-x64
  zip -9 -q "/dist/QBochs-${VERSION_ARG}-x64.zip" qbochs.sys qbochs.inf license.txt
)

# Publish the corresponding source alongside GPL driver binaries.
tar -czf "/dist/QBochs-${VERSION_ARG}-source.tar.gz" \
  -C /qbochs src Dockerfile readme.md license.md

file /package-x86/qbochs.sys
file /package-x64/qbochs.sys
EOF_BUILD

FROM scratch AS artifact
COPY --from=builder /dist/ /

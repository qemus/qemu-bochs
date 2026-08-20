# syntax=docker/dockerfile:1

FROM debian:trixie-slim AS builder

ARG VERSION_ARG="0.0"

RUN <<EOF_BUILD
  set -eu

  mkdir -p /dist

  cat > "/dist/QBochs-${VERSION_ARG}.txt" <<EOF_ARTIFACT
QBochs ${VERSION_ARG}

Build scaffold for the QEMU Bochs display driver.
Driver sources have not been added yet.
EOF_ARTIFACT
EOF_BUILD

FROM scratch AS artifact
COPY --from=builder /dist/ /

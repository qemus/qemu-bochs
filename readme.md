<h1 align="center">QBochs<br />
<div align="center">
  
[![Build]][build_url]
[![Version]][release_url]
[![Size]][release_url]

</div></h1>

QEMU Bochs display driver for Windows NT 5.x, targeting Windows 2000, XP, and Server 2003 on x86 and x64.

QBochs drives QEMU Standard VGA (`PCI\VEN_1234&DEV_1111`) through the Bochs DISPI interface and uses the Windows NT framebuffer display stack. It enumerates only 32-bit modes that fit in the available video memory and requests a write-combined framebuffer mapping, with a normal mapping as fallback.

## Building

The complete build runs in Docker and produces separate x86 and x64 driver packages plus a corresponding source archive:

```bash
docker build --output type=local,dest=output --build-arg VERSION_ARG=0.0 .
```

The build uses the ReactOS GCC/RosBE toolchain and a pinned ReactOS source revision for the NT5 headers and `videoprt` import library. No Microsoft SDK or DDK is required.

## Targets

| Package | Windows versions |
| --- | --- |
| x86 | Windows 2000, Windows XP, Windows Server 2003 |
| x64 | Windows XP Professional x64, Windows Server 2003 x64 |

The driver package binds to QEMU Standard VGA. QEMU `bochs-display` uses the same DISPI register model, but Windows NT5 support for that legacy-free PCI layout still needs guest testing.

## Acknowledgements 🙏

Special thanks to Hervé Poussineau, this project would not exist without his invaluable work on the ReactOS Bochs graphics driver.

## Stars 🌟
[![Stargazers](https://raw.githubusercontent.com/star-stats/stars/refs/heads/data/charts/qemus-qemu-bochs.svg)](https://github.com/qemus/qemu-bochs/stargazers)

[build_url]: https://github.com/qemus/qemu-bochs/
[release_url]: https://github.com/qemus/qemu-bochs/releases/

[Build]: https://github.com/qemus/qemu-bochs/actions/workflows/build.yml/badge.svg
[Size]: https://img.shields.io/badge/size-18.4_MB-steelblue?style=flat&color=066da5
[Version]: https://img.shields.io/github/v/tag/qemus/qemu-bochs?label=version&sort=semver&color=066da5

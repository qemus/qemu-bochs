<h1 align="center">QBochs<br />
<div align="center">
  
[![Build]][build_url]
[![Version]][release_url]
[![Size]][release_url]

</div></h1>

QEMU Bochs display driver for Windows NT 5.x, targeting Windows 2000, XP, and Server 2003 on x86 and x64.

It exists to provide a modern, redistributable display driver for QEMU Standard VGA without falling back to the very limited Cirrus device or depending on a closed third-party driver.

It is derived from the ReactOS Bochs miniport and talks directly to QEMU's Bochs DISPI interface while using the native Windows NT framebuffer display stack.

## Why QBochs? 💡

QEMU's `std` VGA device is a very convenient virtual display adapter: it has a simple linear framebuffer, substantially more video memory than QEMU's emulated Cirrus GD5446, and a small Bochs DISPI register interface for changing modes. The problem is that Windows NT 5.x does not ship with a native driver for it.

For years the practical choices have been:

- use QEMU `cirrus`, which has inbox Windows support but is constrained by the GD5446's small framebuffer and legacy mode limits;
- use a universal VBE driver such as BearWindows `VBEMP`
- use another virtual graphics device whose available NT5 drivers may not match QEMU's implementation;
- remain on the generic VGA fallback.

QBochs takes a narrower approach: support the QEMU/Bochs device directly and keep the implementation small.

## Architecture 🏗️

QBochs is an XPDM video miniport. It does not implement a custom GDI display DLL. Instead it supplies mode-setting and framebuffer access to the Windows NT video stack and uses the operating system's inbox `framebuf.dll` for drawing.

```text
Windows GDI / desktop
        |
        v
  framebuf.dll
  (inbox Windows)
        |
        v
    qbochs.sys
  XPDM miniport
        |
        +---- mode enumeration
        +---- VRAM detection
        +---- framebuffer mapping
        +---- Bochs DISPI mode setting
        |
        v
 QEMU Standard VGA
 PCI 1234:1111
        |
        v
 linear framebuffer
```

The primary target is QEMU Standard VGA:

```text
-vga std
```

QBochs binds to:

```text
PCI\VEN_1234&DEV_1111
```

## Performance strategy ⚡

QBochs deliberately does not emulate a hardware 2D accelerator. On a virtual machine that is not necessarily a disadvantage: the guest CPU is usually hardware-virtualized and extremely fast compared with the machines NT5 originally ran on. The goal is therefore to make software rendering into the framebuffer as inexpensive as possible.

### Write-combined framebuffer 🚀

The miniport first requests the framebuffer through `VideoPortMapMemory()` with `VIDEO_MEMORY_SPACE_P6CACHE`, which asks the NT video port driver for a write-combined mapping. Sequential framebuffer writes can then be combined instead of behaving like ordinary uncached PCI memory writes.

If the video port implementation rejects that mapping, QBochs retries with a normal framebuffer mapping rather than failing the display driver.

### Windows shadow buffering 🪞

The INF installs:

```text
Acceleration.Level = 5
```

On Windows 2000 and later, the framebuffer display stack can use an internal GDI shadow buffer at this setting. Drawing is performed in normal cached system RAM and the changed output is copied to video memory, which avoids making every GDI operation draw directly into slow framebuffer memory.

This is the same NT5 optimization documented and used by BearWindows `VBEMP`. Combined with write combining, the intended path is:

```text
GDI drawing
    |
    v
cached system-memory shadow buffer
    |
    v
write-combined framebuffer copy
    |
    v
QEMU std VGA
```

The two optimizations complement each other: shadow buffering keeps most drawing in cacheable RAM, while write combining makes the eventual transfer to the linear framebuffer cheaper.

## QBochs vs. BearWindows ⚖️

BearWindows has done extensive work on Windows NT framebuffer and Cirrus drivers, and QBochs intentionally borrows the same general performance ideas where they apply. The projects solve different problems, however.

| | QBochs | BearWindows VBEMP | BearWindows CIRRUS.NT |
| --- | --- | --- | --- |
| Virtual device | QEMU `std` / Bochs DISPI | Any supported VBE adapter | Cirrus Logic GD54xx, including QEMU GD5446 |
| Hardware scope | QEMU-specific | Broad physical + virtual VBE hardware | Cirrus-specific |
| Mode setting | Direct Bochs DISPI registers | VBE/VESA BIOS interface | Cirrus-specific registers |
| Rendering model | Windows framebuffer GDI | Windows framebuffer GDI | Hardware-specific Cirrus driver |
| Shadow buffering | Yes, via NT5 `framebuf.dll` | Yes | Driver-specific |
| Write combining | Requested directly by miniport | Supported/configurable | Driver-specific |
| Hardware 2D blitter | No | No | Potentially yes |
| 3D acceleration | No | No | No |
| QEMU framebuffer size | `std` is typically much larger | Depends on adapter | QEMU GD5446 is limited to 4 MiB of real VRAM |
| High-resolution 32-bit modes | Good fit for the device | Depends on VBE adapter | Limited by Cirrus VRAM |
| BIOS dependency after driver load | No for DISPI mode setting | Yes for VBE operations | No VBE dependency for native Cirrus operation |
| Source available | Yes | Depends on release/package | Depends on release/package |
| QBochs project redistribution goal | Yes, GPL driver source + binaries | Separate project/license | Separate project/license |
| x86 build | Yes | Yes | Yes |
| x64 QBochs target | Yes | Separate BearWindows support varies by release | Separate BearWindows support varies by release |

## Supported systems 💻

| Package | Windows versions |
| --- | --- |
| x86 | Windows 2000, Windows XP, Windows Server 2003 |
| x64 | Windows XP Professional x64, Windows Server 2003 x64 |

## Current limitations ⚠️

- 32-bit color modes only.
- No Direct3D or OpenGL hardware acceleration.
- No hardware BitBlt/rectangle-fill acceleration.
- No hardware cursor implementation.

## Installation 📦

Configure an x86/x64 QEMU VM with Standard VGA:

```text
-vga std
```

Then install `qbochs.inf` from the package matching the guest architecture. The INF uses the Windows installation's own `framebuf.dll`; QBochs does not redistribute that Microsoft component.

Because the current packages are unsigned, Windows versions that enforce driver-signing policy may require the appropriate test/development configuration.

## Origins 🙏

QBochs is derived from the ReactOS Bochs graphics miniport written by **Hervé Poussineau**. The original driver already provided the important foundation: direct Bochs DISPI access, QEMU/Bochs PCI resource handling, VRAM-aware mode enumeration, and support for both the legacy I/O and MMIO register layouts.

QBochs adapts that work into a standalone QEMU-focused NT5 driver project and build pipeline, adds compatibility choices for Microsoft Windows NT5, and applies framebuffer performance settings intended for real Windows 2000/XP/2003 guests.

## References 🔗

- [ReactOS Bochs miniport](https://github.com/reactos/reactos/tree/master/win32ss/drivers/miniport/bochs)
- [QEMU Standard VGA specification](https://www.qemu.org/docs/master/specs/standard-vga.html)
- [BearWindows Universal VBE NT Display Driver Project](https://bearwindows.zcm.com.au/vbemp.htm)

## Stars 🌟

[![Stargazers](https://raw.githubusercontent.com/star-stats/stars/refs/heads/data/charts/qemus-qemu-bochs.svg)](https://github.com/qemus/qemu-bochs/stargazers)

[build_url]: https://github.com/qemus/qemu-bochs/
[release_url]: https://github.com/qemus/qemu-bochs/releases/

[Build]: https://github.com/qemus/qemu-bochs/actions/workflows/build.yml/badge.svg
[Size]: https://img.shields.io/badge/size-18.4_MB-steelblue?style=flat&color=066da5
[Version]: https://img.shields.io/github/v/tag/qemus/qemu-bochs?label=version&sort=semver&color=066da5

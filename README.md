# IMX708 Driver — Sony IMX708 Camera Sensor Linux Driver

**Linux kernel module, userspace C library, and diagnostic tools for the Sony IMX708 12MP CMOS image sensor** (Raspberry Pi Camera Module 3).

## Contents

| Component | Description |
|---|---|
| `src/` | Kernel module source (V4L2 sub-device, I2C driver) |
| `include/` | Kernel headers (register map, platform abstraction, UAPI) |
| `lib/` | Userspace C library (`libimx708`) |
| `test/` | Test applications (suite, CLI, stress, capture) |
| `python/` | Python ctypes bindings |
| `dts/` | Device tree overlay for Raspberry Pi |
| `docs/` | Documentation |
| `tools/` | Static analysis scripts |
| `packaging/` | Debian/RPM/IPK packaging |
| `systemd/` | Systemd service for module loading |
| `config/` | Default configuration |
| `docker/` | Docker cross-build environment |

## Quick Start

```bash
# Build everything
make

# Build only the kernel module
make module

# Build only the userspace library
make lib

# Install on target
sudo make install

# Cross-compile for Raspberry Pi
make PLATFORM=rpi4
```

## Requirements

- Linux kernel headers (`linux-headers-$(uname -r)`)
- GCC, make
- For Raspberry Pi cross-build: `gcc-aarch64-linux-gnu`

## License

GPL-2.0-only

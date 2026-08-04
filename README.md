# 📷 IMX708 Driver — Sony IMX708 12MP Camera Sensor Linux Kernel Driver

> **Production-grade Linux kernel module, userspace C library, Python bindings, and diagnostic toolchain for the Sony IMX708 11.9MP CMOS image sensor** — the sensor powering the Raspberry Pi Camera Module 3.

[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE)
[![Kernel](https://img.shields.io/badge/Linux-6.1%2B-critical)](Kbuild)
[![Platform](https://img.shields.io/badge/Platform-RPi4%20%7C%20RPi5%20%7C%20x86__64-success)](Makefile)
[![Build](https://img.shields.io/badge/Build-Make%20%7C%20Docker%20cross--compile-9cf)](docker/)

---

## 🏗️ Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                        USERSPACE                                     │
│                                                                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐  ┌──────────┐ │
│  │  imx708_test │  │  imx708_cli │  │imx708_stress │  │imx708_cap│ │
│  │  (test suite)│  │  (CLI tool) │  │ (stress test)│  │(capture) │ │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘  └────┬─────┘ │
│         │                 │                 │               │        │
│         └─────────────────┼─────────────────┴───────────────┘        │
│                            │                                          │
│                    ┌───────▼────────┐                                 │
│                    │   libimx708    │  (static/shared C library)     │
│                    │  (C API)       │                                 │
│                    └───────┬────────┘                                 │
│                            │                                          │
│                    ┌───────▼────────┐                                 │
│                    │  python/       │  (Python ctypes bindings)      │
│                    │  imx708.py     │                                 │
│                    └───────┬────────┘                                 │
│                            │                                          │
├────────────────────────────┼──────────────────────────────────────────┤
│                        KERNEL SPACE                                   │
│                            │                                          │
│                    ┌───────▼────────┐                                 │
│                    │  /dev/imx708N  │  (char device, ioctl ABI)       │
│                    └───────┬────────┘                                 │
│                            │                                          │
│                    ┌───────▼────────┐                                 │
│                    │  imx708.ko     │  (V4L2 sub-device driver)      │
│                    │                │                                 │
│  ┌──────────────┐  │  ┌──────────┐ │  ┌──────────────┐  ┌──────────┐ │
│  │  imx708_main │  │  │imx708_pm │ │  │ imx708_irq  │  │imx708_sys│ │
│  │  (probe/init)│  │  │(PM ops)  │ │  │ (IRQ hdlr)  │  │(sysfs)   │ │
│  └──────────────┘  │  └──────────┘ │  └──────────────┘  └──────────┘ │
│  ┌──────────────┐  │  ┌──────────┐ │  ┌──────────────┐  ┌──────────┐ │
│  │imx708_platfrm│  │  │imx708_chr│ │  │imx708_debug │  │imx708_tra│ │
│  │(SoC ops)     │  │  │(chardev) │ │  │(debugfs)    │  │(trace)   │ │
│  └──────────────┘  │  └──────────┘ │  └──────────────┘  └──────────┘ │
│                    └───────┬────────┘                                 │
│                            │                                          │
│                    ┌───────▼────────┐                                 │
│                    │   I2C bus      │  (regmap, 16-bit addressing)    │
│                    └───────┬────────┘                                 │
│                            │                                          │
├────────────────────────────┼──────────────────────────────────────────┤
│                         HARDWARE                                      │
│                    ┌───────▼────────┐                                 │
│                    │  Sony IMX708   │  (MIPI CSI-2, 4-lane)           │
│                    │  11.9MP Sensor │                                 │
│                    └────────────────┘                                 │
└────────────────────────────────────────────────────────────────────────┘
```

---

## ✨ Features

### 🧩 Kernel Module (`imx708.ko`)
- **Full V4L2 sub-device** — `s_stream`, `get_fmt`/`set_fmt`, `enum_mbus_code`, `enum_frame_size`, `g_frame_interval`
- **30+ V4L2 controls** — analog/digital gain, exposure, HDR, test patterns, white balance, brightness, contrast, saturation, hue, gamma, sharpness, 3A lock, scene modes, color effects, zoom/pan/tilt, power line frequency, backlight compensation, ISO
- **Platform abstraction** — SoC-specific ops via `imx708_hw_ops` interface (RPi, RPi Wide variants)
- **I2C regmap** — 16-bit big-endian register addressing with retry logic
- **Runtime PM** — autosuspend with full regulator + GPIO sequencing
- **Interrupt handling** — threaded IRQ with atomic event latching (optional, DT-gated)
- **Sysfs ABI** — temperature, streaming state, frame count, chip ID, driver version
- **Char device** — `/dev/imx708N` with ioctl-based control (used by libimx708)
- **Debugfs** — per-instance register dump, IRQ counters, trace ring buffer
- **FTrace integration** — tracepoints for probe, remove, power, stream, IRQ events
- **Device tree** — overlay for Raspberry Pi (`dts/imx708-rpi.dts`)
- **Multi-platform** — native x86_64 build + cross-compile for RPi4/RPi5

### 📚 Userspace C Library (`libimx708`)
- **Static + shared** — `libimx708.a` and `libimx708.so.0`
- **Full API coverage** — open/close, get/set gain, exposure, HDR, test patterns, image processing, capture frames, register read/write, profiles
- **Thread-safe** — internal mutex protection
- **pkg-config** — `libimx708.pc` for easy integration
- **Symbol visibility** — version script for ABI stability

### 🐍 Python Bindings
- **ctypes-based** — no compilation needed, wraps `libimx708.so`
- **Full API** — all sensor controls accessible from Python
- **Example-ready** — drop into any Python project

### 🧪 Test Suite
| Tool | Purpose |
|------|---------|
| `imx708_test` | Comprehensive test suite (unit + integration) |
| `imx708_cli` | Interactive CLI for manual sensor control |
| `imx708_stress` | Long-duration stress testing (hours of streaming) |
| `imx708_capture` | Frame capture to file (RAW10, JPEG) |

### 📦 Packaging
- **Debian** — `packaging/debian/` (control, rules, changelog)
- **RPM** — `packaging/rpm/imx708.spec`
- **IPK** — `packaging/ipk/CONTROL/control` (OpenWrt)
- **Systemd** — `systemd/imx708.service` (auto-load on boot)
- **Docker** — cross-build environment for RPi4 (`docker/Dockerfile.rpi4`)

---

## 📋 Sensor Specifications

| Parameter | Value |
|-----------|-------|
| **Sensor** | Sony IMX708-AAJH5-C |
| **Resolution** | 11.9 MP (4608 × 2592 active) |
| **Pixel Size** | 1.4 µm |
| **Optical Format** | 1/2.43" |
| **Output** | MIPI CSI-2 (4-lane / 2-lane) |
| **Bit Depth** | 10-bit RAW (RAW10) |
| **Frame Rate** | Up to 56 fps (full res), 120 fps (720p) |
| **HDR** | 2-exposure line-interleaved (up to 4× ratio) |
| **PDAF** | On-chip phase detection (12×16 grid) |
| **I2C Address** | 0x1a (fixed) |
| **Input Clock** | 24 MHz |
| **Supplies** | vana1 (2.8V), vana2 (1.8V), vdig (1.1V), vddl (1.8V) |
| **Temperature Range** | −20°C to +80°C (signed 8-bit readout) |

---

## 🚀 Quick Start

### Build Everything
```bash
# Native build (on Raspberry Pi or x86_64 with kernel headers)
make

# Build only the kernel module
make module

# Build only the userspace library
make lib

# Build only the test applications
make test

# Cross-compile for Raspberry Pi 4
make PLATFORM=rpi4 KERNEL_SRC=~/pi-kernel
```

### Install on Target
```bash
sudo make install

# Load the module
sudo modprobe imx708

# Verify
ls /dev/imx708*
cat /sys/bus/i2c/devices/*-001a/temperature
```

### Use the CLI
```bash
# Interactive mode
./build/native/test/imx708_cli

# Get sensor status
./build/native/test/imx708_cli --status

# Set gain and exposure
./build/native/test/imx708_cli --set-gain 480 --set-exposure 1000

# Capture a frame
./build/native/test/imx708_capture --output frame.raw --width 4608 --height 2592
```

### Run the Test Suite
```bash
./build/native/test/imx708_test --verbose
```

### Stress Test
```bash
# 1-hour stress test with 4K streaming
./build/native/test/imx708_stress --duration 3600 --width 4608 --height 2592
```

---

## 📁 Project Structure

```
imx708-driver/
├── src/                          # Kernel module source
│   ├── imx708_main.c            # I2C probe/remove, V4L2 subdev, controls
│   ├── imx708_platform.c        # SoC-specific ops (RPi, RPi Wide)
│   ├── imx708_pm.c              # Runtime PM callbacks
│   ├── imx708_irq.c             # Interrupt handling (optional)
│   ├── imx708_sysfs.c           # Sysfs attributes
│   ├── imx708_chardev.c         # Char device / ioctl ABI
│   ├── imx708_debugfs.c         # Debugfs interface
│   ├── imx708_trace.c/h         # FTrace tracepoints
│   └── imx708_trace.h
├── include/                      # Kernel headers
│   ├── imx708_platform.h        # Platform data, hw_ops interface
│   ├── imx708_regs.h            # Register map (verified from upstream)
│   └── imx708_uapi.h            # Userspace ioctl API
├── lib/                          # Userspace C library
│   ├── include/libimx708.h      # Public API header
│   ├── src/libimx708.c          # Library implementation
│   ├── libimx708.pc.in          # pkg-config template
│   └── libimx708.version        # Symbol version script
├── python/                       # Python ctypes bindings
│   └── imx708.py
├── test/                         # Test applications
│   ├── imx708_test.c            # Test suite
│   ├── imx708_cli.c             # Interactive CLI
│   ├── imx708_stress.c          # Stress tester
│   └── imx708_capture.c         # Frame capture tool
├── dts/                          # Device tree
│   └── imx708-rpi.dts           # Raspberry Pi overlay
├── config/                       # Configuration
│   └── imx708.conf
├── docs/                         # Documentation
│   ├── 01-build.md              # Build guide
│   ├── 02-install.md            # Installation guide
│   ├── 03-sysfs-abi.md          # Sysfs ABI reference
│   ├── 04-ioctl-api.md          # IOCTL API reference
│   ├── 05-library-api.md        # Library API reference
│   ├── 06-testing.md            # Testing guide
│   ├── 07-debugging.md          # Debugging guide
│   └── 08-porting.md            # Porting guide
├── packaging/                    # Distribution packages
│   ├── debian/                  # Debian packaging
│   ├── rpm/                     # RPM packaging
│   └── ipk/                     # OpenWrt IPK packaging
├── systemd/                      # Systemd services
│   ├── imx708.service           # Module loader service
│   └── imx708-grpc.service      # gRPC server service
├── docker/                       # Docker cross-build
│   ├── Dockerfile.rpi4          # RPi4 cross-build image
│   └── build.sh                 # Docker build helper
├── scripts/                      # Utility scripts
│   └── install.sh
├── tools/                        # Static analysis
│   ├── checkpatch-wrapper.sh    # Kernel checkpatch wrapper
│   ├── run-static-analysis.sh   # Sparse, smatch, coccinelle
│   └── verify-build.sh          # Build verification
├── Kbuild                        # Kernel module build system
├── Makefile                      # Top-level build
├── VERSION                       # Version file
├── LICENSE                       # GPL-2.0-only
└── README.md                     # This file
```

---

## 🎮 V4L2 Controls Reference

| Control ID | Type | Range | Default | Description |
|-----------|------|-------|---------|-------------|
| `V4L2_CID_ANALOGUE_GAIN` | Integer | 0–960 | 0 | Analog gain (sensor units) |
| `V4L2_CID_DIGITAL_GAIN` | Integer | 256–65535 | 256 | Digital gain (1.0 = 0x0100) |
| `V4L2_CID_EXPOSURE` | Integer | 8–65487 | 1600 | Exposure in line units |
| `V4L2_CID_AUTO_EXPOSURE_BIAS` | Int Menu | −3.0 to +3.0 EV | 0 EV | Exposure compensation |
| `V4L2_CID_ISO_SENSITIVITY` | Int Menu | 100–3200 | 100 | ISO sensitivity |
| `V4L2_CID_ISO_SENSITIVITY_AUTO` | Menu | Auto/Manual | Auto | ISO auto mode |
| `V4L2_CID_3A_LOCK` | Bitmask | 0–7 | 0 | Lock AE/AWB/focus |
| `V4L2_CID_BRIGHTNESS` | Integer | −255–255 | 0 | Black level offset |
| `V4L2_CID_CONTRAST` | Integer | 0–255 | 128 | Contrast |
| `V4L2_CID_SATURATION` | Integer | 0–255 | 128 | Saturation |
| `V4L2_CID_HUE` | Integer | −180–180 | 0 | Hue |
| `V4L2_CID_GAMMA` | Integer | 0–255 | 128 | Gamma correction |
| `V4L2_CID_SHARPNESS` | Integer | 0–15 | 0 | Sharpness |
| `V4L2_CID_AUTO_WHITE_BALANCE` | Boolean | 0/1 | 1 | Auto white balance |
| `V4L2_CID_WHITE_BALANCE_TEMPERATURE` | Integer | 2000–8000K | 5000K | WB temperature |
| `V4L2_CID_TEST_PATTERN` | Menu | 0–7 | 0 | Test pattern generator |
| `V4L2_CID_HFLIP` | Boolean | 0/1 | 0 | Horizontal flip |
| `V4L2_CID_VFLIP` | Boolean | 0/1 | 0 | Vertical flip |
| `V4L2_CID_POWER_LINE_FREQUENCY` | Menu | 0–3 | 0 | Anti-flicker |
| `V4L2_CID_BACKLIGHT_COMPENSATION` | Integer | 0–2 | 0 | Backlight compensation |
| `V4L2_CID_SCENE_MODE` | Menu | 0–8 | 0 | Scene mode presets |
| `V4L2_CID_COLORFX` | Menu | 0–9 | 0 | Color effects |
| `V4L2_CID_ZOOM_ABSOLUTE` | Integer | 0–100 | 0 | Digital zoom |
| `V4L2_CID_PAN_ABSOLUTE` | Integer | −100–100 | 0 | Pan |
| `V4L2_CID_TILT_ABSOLUTE` | Integer | −100–100 | 0 | Tilt |

---

## 📐 Supported Modes

| Mode | Resolution | FPS | Pixel Rate | HBlank | VBlank | Binning |
|------|-----------|-----|------------|--------|--------|---------|
| 0 | 4608 × 2592 | 56 | 595.2 MHz | 15648 | 58 | None |
| 1 | 2304 × 1296 | 56 | 585.6 MHz | 7824 | 58 | 2×2 |
| 2 | 1536 × 864 | 120 | 566.4 MHz | 5216 | 58 | 2×2 |
| 3 | 4608 × 2592 (HDR) | 30 | 777.6 MHz | 15648 | 58 | None |

---

## 🔧 Sysfs ABI

All attributes are under `/sys/bus/i2c/devices/<bus>-001a/`:

| Attribute | Access | Description |
|-----------|--------|-------------|
| `temperature` | RO | Sensor temperature (°C, signed 8-bit) |
| `streaming` | RO | Streaming state (0/1) |
| `frame_count` | RO | Total frames captured |
| `chip_id` | RO | Chip ID (0x0708) |
| `driver_version` | RO | Driver version string |
| `power_control` | RW | Force power on/off (debug) |

---

## 🐳 Docker Cross-Compilation

```bash
# Build the cross-compile image
cd docker
./build.sh

# Build the driver for RPi4
make PLATFORM=rpi4 KERNEL_SRC=/path/to/rpi-kernel
```

---

## 📦 Packaging

### Debian/Ubuntu
```bash
cd packaging/debian
dpkg-buildpackage -us -uc
sudo dpkg -i ../imx708-driver_*.deb
```

### RPM (Fedora/CentOS)
```bash
rpmbuild -ba packaging/rpm/imx708.spec
sudo rpm -ivh ~/rpmbuild/RPMS/aarch64/imx708-driver-*.rpm
```

### OpenWrt IPK
```bash
cd packaging/ipk
# Build with OpenWrt SDK
```

---

## 🧪 Testing & Validation

```bash
# Run the full test suite
./build/native/test/imx708_test

# Run with verbose output
./build/native/test/imx708_test --verbose

# Run specific tests
./build/native/test/imx708_test --test gain,exposure,hdr

# Stress test (1 hour)
./build/native/test/imx708_stress --duration 3600

# Capture test frames
./build/native/test/imx708_capture --count 100 --output /tmp/frames/
```

### Static Analysis
```bash
# Kernel checkpatch
./tools/checkpatch-wrapper.sh

# Sparse, smatch, coccinelle
./tools/run-static-analysis.sh

# Build verification
./tools/verify-build.sh
```

---

## 🔌 Integration with Other Projects

This driver is the foundation for the **IMX708 ecosystem**:

```
┌─────────────────────────────────────────────────────────────┐
│                    IMX708 Ecosystem                          │
│                                                              │
│  ┌──────────────┐    ┌──────────────┐    ┌──────────────┐   │
│  │ imx708-driver│◄──►│imx708-server │◄──►│  imx708-gui  │   │
│  │ (kernel mod) │    │ (gRPC daemon)│    │ (PySide6 GUI)│   │
│  └──────────────┘    └──────────────┘    └──────────────┘   │
│         │                                                   │
│         ▼                                                   │
│  ┌──────────────┐                                           │
│  │  libimx708   │  (shared C library for all clients)       │
│  └──────────────┘                                           │
└─────────────────────────────────────────────────────────────┘
```

- **imx708-server** — C++ gRPC server that wraps this driver's ioctl ABI
- **imx708-gui** — Cross-platform PySide6 desktop GUI with macOS-like design

---

## 📄 License

This project is licensed under the **GNU General Public License v2.0-only** — see [LICENSE](LICENSE).

```
Copyright (C) 2026 SoC Centric
Author: Sandesh <sandesh@soccentric.com>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License version 2 as
published by the Free Software Foundation.
```

---

## 🙏 Acknowledgements

- **Raspberry Pi Foundation** — upstream kernel driver reference (rpi-6.6.y)
- **Sony Semiconductor** — IMX708-AAJH5-C datasheet
- **Linux Media Subsystem** — V4L2 framework documentation

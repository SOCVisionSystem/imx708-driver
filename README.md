# imx708-driver

Linux kernel module and userspace C library for the Sony IMX708 11.9MP
CMOS image sensor — the sensor powering the Raspberry Pi Camera Module 3.

## Summary

The imx708-driver is a production-grade Linux kernel module that implements
a full V4L2 sub-device for the Sony IMX708 camera sensor. It provides
complete control over the sensor through the standard V4L2 framework, a
character device interface with 16 ioctls, sysfs attributes for runtime
monitoring, and debugfs for diagnostics. The driver is built on a platform
abstraction layer that separates SoC-specific hardware details from the
core logic, making it straightforward to add support for new platforms
without modifying the core driver code. A thread-safe C library wraps the
ioctl interface for application developers, with both static and shared
builds, pkg-config integration, and symbol versioning for ABI stability.
The driver handles I2C register access via regmap with 16-bit big-endian
addressing, runtime power management with full regulator and GPIO sequencing
for four power supplies, and threaded interrupt handling for event-driven
operation. A comprehensive test suite includes unit tests, an interactive
CLI tool, a multi-threaded stress tester for lockdep and KCSAN validation,
and a frame capture application. The project supports native x86_64 builds
and cross-compilation for Raspberry Pi 4 and 5, with Docker-based cross-build
environments, Debian/RPM/IPK packaging, and systemd integration for production
deployment. Every source file includes SPDX license headers, and the
documentation covers build instructions, installation, sysfs ABI, ioctl API,
library API, testing procedures, debugging techniques, and a porting guide
for adding support for new SoC platforms.

## Features

- Full V4L2 sub-device with s_stream, get_fmt/set_fmt, enum_mbus_code,
  enum_frame_size, and g_frame_interval operations
- 30+ V4L2 controls including analog gain, digital gain, exposure, HDR,
  test patterns, white balance, brightness, contrast, saturation, hue,
  gamma, sharpness, 3A lock, scene modes, color effects, zoom, pan, tilt,
  power line frequency, backlight compensation, and ISO
- Platform abstraction layer via imx708_hw_ops interface for adding new
  SoC variants without changing core driver code
- I2C regmap with 16-bit big-endian register addressing and retry logic
- Runtime power management with autosuspend and full regulator sequencing
  for vana1, vana2, vdig, and vddl supplies
- GPIO-controlled reset and power sequencing with configurable delays
- Threaded interrupt handling with IRQF_ONESHOT for I2C-based IRQ
  acknowledgement
- Char device interface at /dev/imx708N with 16 properly-encoded ioctls
  using fixed-width types for 32/64-bit compatibility
- Sysfs attributes for temperature, streaming state, frame count, chip ID,
  driver version, gain, exposure, test pattern, PLL lock, HDR mode, and
  current sensor mode
- Debugfs interface with per-instance register dump, IRQ event counters,
  error counters, driver state, and fault injection controls
- FTrace integration with tracepoints for probe, remove, power, stream,
  IRQ, frame, error, and register access events
- Thread-safe C library (libimx708) with opaque handle, internal mutex,
  and full API coverage for all sensor controls
- Static and shared library builds with symbol versioning for ABI stability
- pkg-config integration via libimx708.pc for easy project integration
- Frame capture API with PGM save, burst capture, streaming callbacks,
  configuration profiles, and auto-exposure helper
- Comprehensive test suite with unit tests, interactive CLI, multi-threaded
  stress tester, and frame capture tool
- Device tree overlay for Raspberry Pi with compatible strings for standard
  and wide camera module variants
- Multi-platform build support for native x86_64 and cross-compilation for
  Raspberry Pi 4 and 5
- Docker cross-build environment with public toolchains and pinned kernel
  sources for reproducible builds
- Debian, RPM, and IPK packaging with runtime, development, and tools
  package splits
- Systemd service units for module auto-load and gRPC server integration
- Static analysis tooling with checkpatch, sparse, smatch, and coccinelle
  wrappers
- SPDX license headers on every source file with GPL-2.0-only licensing
- Comprehensive documentation with 8 dedicated guides covering build,
  install, sysfs ABI, ioctl API, library API, testing, debugging, and
  porting to new platforms
- Python ctypes bindings for accessing the sensor from Python without
  compiling C code

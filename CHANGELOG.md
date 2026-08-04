# Changelog

## 0.1.0 (2026-08-04)

### Added
- Initial V4L2 sub-device driver with 30+ controls
- Platform abstraction layer (imx708_hw_ops) for RPi and RPi Wide
- I2C regmap with 16-bit big-endian register addressing
- Runtime PM with regulator and GPIO sequencing
- Char device interface (/dev/imx708N) with 16 ioctls
- Sysfs ABI (temperature, streaming, frame count, chip ID, etc.)
- Debugfs interface (register dump, IRQ counters, fault injection)
- FTrace integration (probe, remove, power, stream, IRQ events)
- Thread-safe C library (libimx708) with static and shared builds
- Test suite: unit tests, CLI tool, stress tester, capture tool
- Debian, RPM, and IPK packaging
- Docker cross-build for Raspberry Pi 4
- Systemd services for module loading and gRPC server
- Comprehensive documentation (8 docs files)

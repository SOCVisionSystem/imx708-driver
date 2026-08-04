# Build

## Prerequisites

### Native build (x86_64)

```bash
sudo apt-get install build-essential linux-headers-$(uname -r)
```

### Cross-build for Raspberry Pi

```bash
sudo apt-get install gcc-aarch64-linux-gnu
# Or use Docker (see below)
```

## Quick start

```bash
# Build everything natively
make all

# Build only the kernel module
make module

# Build only the userspace library
make lib

# Build only the test applications
make test

# Clean
make clean
```

## Cross-build for Raspberry Pi 4

### Using Docker (recommended)

```bash
# Build the Docker image
make PLATFORM=rpi4 docker-image

# Build everything inside the container
make PLATFORM=rpi4 all

# Or use the helper script
./docker/build.sh rpi4 all
```

### Using local cross toolchain

```bash
# Set up kernel source
git clone --branch rpi-6.6.y https://github.com/raspberrypi/linux.git ~/pi-kernel
cd ~/pi-kernel
make ARCH=arm64 bcm2711_defconfig
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- modules_prepare

# Build the module
cd ~/pi/imx708
make PLATFORM=rpi4 KERNEL_SRC=~/pi-kernel module
```

## Kernel version

Target: **6.6 LTS** (Raspberry Pi kernel branch `rpi-6.6.y`).

The Docker image pins a specific commit for reproducible builds. To use a different kernel:

```bash
make KERNEL_SRC=/path/to/other/kernel module
```

## Build output

All build artifacts go to `build/<platform>/`:

```
build/
├── native/
│   ├── module/          # .ko, .o, .mod.c
│   ├── lib/             # .so, .a
│   └── test/            # test binaries
└── rpi4/
    ├── module/
    ├── lib/
    └── test/
```

#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
#
# install.sh - Install IMX708 driver and tools on the target system
#
# Copyright (C) 2026 SoC Centric
#
# Usage:
#   sudo ./scripts/install.sh [--prefix /usr/local]
#
# Installs:
#   - Kernel module (imx708.ko) from driver/
#   - Shared library (libimx708.so) from build/
#   - Development files (headers, static lib, pkg-config)
#   - Test and capture tools
#   - Python bindings
#   - Systemd services
#   - Default configuration
#   - Udev rules
#

set -euo pipefail

PREFIX="${PREFIX:-/usr/local}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== IMX708 Installer ==="
echo "Prefix: ${PREFIX}"
echo ""

# Check for root
if [ "$(id -u)" -ne 0 ]; then
    echo "ERROR: This script must be run as root (sudo)"
    exit 1
fi

# 1. Kernel module
echo "[1/7] Installing kernel module..."
if [ -f "${PROJECT_DIR}/driver/imx708.ko" ]; then
    KVER="$(uname -r)"
    MODDIR="/lib/modules/${KVER}/extra"
    mkdir -p "${MODDIR}"
    cp "${PROJECT_DIR}/driver/imx708.ko" "${MODDIR}/"
    depmod -a
    echo "  Module installed to ${MODDIR}/imx708.ko"
else
    echo "  SKIP: imx708.ko not found (run 'make driver' first)"
fi

# 2. Shared library
echo "[2/7] Installing shared library..."
mkdir -p "${PREFIX}/lib"
if [ -f "${PROJECT_DIR}/build/native/lib/libimx708.so.0.0.1.0" ]; then
    cp "${PROJECT_DIR}/build/native/lib/libimx708.so.0.0.1.0" "${PREFIX}/lib/"
    ln -sf libimx708.so.0.0.1.0 "${PREFIX}/lib/libimx708.so.0"
    ln -sf libimx708.so.0.0.1.0 "${PREFIX}/lib/libimx708.so"
    ldconfig
    echo "  Library installed to ${PREFIX}/lib/"
else
    echo "  SKIP: library not found (run 'make driver-lib' first)"
fi

# 3. Development files
echo "[3/7] Installing development files..."
mkdir -p "${PREFIX}/include"
cp "${PROJECT_DIR}/driver/lib/include/libimx708.h" "${PREFIX}/include/"
if [ -f "${PROJECT_DIR}/build/native/lib/libimx708.a" ]; then
    cp "${PROJECT_DIR}/build/native/lib/libimx708.a" "${PREFIX}/lib/"
fi
mkdir -p "${PREFIX}/lib/pkgconfig"
cp "${PROJECT_DIR}/driver/lib/libimx708.pc.in" "${PREFIX}/lib/pkgconfig/libimx708.pc"
echo "  Headers and static library installed"

# 4. Tools
echo "[4/7] Installing tools..."
mkdir -p "${PREFIX}/bin"
for tool in imx708_test imx708_cli imx708_stress imx708_capture; do
    if [ -f "${PROJECT_DIR}/build/native/test/${tool}" ]; then
        cp "${PROJECT_DIR}/build/native/test/${tool}" "${PREFIX}/bin/"
        echo "  ${tool} installed"
    fi
done

# 5. Python bindings
echo "[5/7] Installing Python bindings..."
mkdir -p "${PREFIX}/lib/python3/dist-packages"
if [ -f "${PROJECT_DIR}/driver/python/imx708.py" ]; then
    cp "${PROJECT_DIR}/driver/python/imx708.py" "${PREFIX}/lib/python3/dist-packages/"
    echo "  Python bindings installed"
fi

# 6. Systemd services
echo "[6/7] Installing systemd services..."
mkdir -p /etc/imx708
cp "${PROJECT_DIR}/config/imx708.conf" /etc/imx708/
for svc in imx708.service imx708-grpc.service; do
    if [ -f "${PROJECT_DIR}/systemd/${svc}" ]; then
        cp "${PROJECT_DIR}/systemd/${svc}" /etc/systemd/system/
        echo "  ${svc} installed"
    fi
done
systemctl daemon-reload

# 7. Udev rules
echo "[7/7] Installing udev rules..."
cat > /etc/udev/rules.d/99-imx708.rules << 'EOF'
# IMX708 camera sensor - allow video group access
KERNEL=="imx708[0-9]*", MODE="0660", GROUP="video"
EOF
udevadm control --reload-rules
echo "  Udev rules installed"

echo ""
echo "=== Installation complete ==="
echo "Load module:  sudo modprobe imx708"
echo "Test:         imx708_test /dev/imx7080"
echo "Capture:      imx708_capture /dev/imx7080 snap"
echo "Python:       python3 -c \"from imx708 import Camera; c = Camera(); print(c.status)\""
echo "gRPC server:  sudo imx708-server --device /dev/imx7080"
echo "GUI client:   python3 gui/imx708_client.py --server <pi-ip>:50051"

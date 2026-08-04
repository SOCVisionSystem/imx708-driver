#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
#
# checkpatch-wrapper.sh - Run checkpatch.pl on IMX708 kernel sources
#
# Copyright (C) 2026 SoC Centric
#
# Usage:
#   ./tools/checkpatch-wrapper.sh [kernel-source-dir]
#

set -euo pipefail

KDIR="${1:-/lib/modules/$(uname -r)/build}"
CHECKPATCH="${KDIR}/scripts/checkpatch.pl"

if [ ! -f "${CHECKPATCH}" ]; then
    echo "checkpatch.pl not found at ${CHECKPATCH}"
    echo "Install kernel source or provide path: $0 <kernel-source-dir>"
    exit 1
fi

SOURCES=(
    src/imx708_main.c
    src/imx708_platform.c
    src/imx708_chardev.c
    src/imx708_sysfs.c
    src/imx708_debugfs.c
    src/imx708_irq.c
    src/imx708_pm.c
)

echo "=== Running checkpatch.pl on IMX708 sources ==="
echo ""

FAILED=0
for src in "${SOURCES[@]}"; do
    if [ ! -f "${src}" ]; then
        echo "SKIP: ${src} not found"
        continue
    fi

    echo "--- ${src} ---"
    if perl "${CHECKPATCH}" --strict --no-tree -f "${src}"; then
        echo "PASS"
    else
        FAILED=$((FAILED + 1))
    fi
    echo ""
done

echo "=== Results: ${FAILED} files with warnings ==="
exit ${FAILED}

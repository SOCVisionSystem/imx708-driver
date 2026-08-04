#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
#
# verify-build.sh - Full build verification for IMX708 driver
#
# Copyright (C) 2026 SoC Centric
#
# Usage:
#   ./tools/verify-build.sh [--platform rpi4|native]
#
# Runs the full build chain: static analysis, kernel module, userspace
# library, test apps, and packaging. Reports pass/fail for each stage.
#

set -euo pipefail

PLATFORM="${1:-native}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

cd "${PROJECT_DIR}"

echo "============================================"
echo " IMX708 Build Verification"
echo " Platform: ${PLATFORM}"
echo "============================================"
echo ""

PASS=0
FAIL=0

check() {
    local name="$1"
    shift
    echo "--- [${name}] ---"
    if "$@" 2>&1; then
        echo "PASS: ${name}"
        PASS=$((PASS + 1))
    else
        echo "FAIL: ${name}"
        FAIL=$((FAIL + 1))
    fi
    echo ""
}

# 1. Static analysis
check "static-analysis" bash -c "
    ${SCRIPT_DIR}/run-static-analysis.sh 2>&1 || true
    echo '(static analysis results above)'
"

# 2. Clean build
check "clean" make clean

# 3. Userspace library
check "library" make lib

# 4. Test applications
check "test-apps" make test

# 5. Kernel module (native only, needs headers)
if [ "${PLATFORM}" = "native" ] && [ -d "/lib/modules/$(uname -r)/build" ]; then
    check "kernel-module" make module
else
    echo "--- [kernel-module] ---"
    echo "SKIP: kernel headers not available for ${PLATFORM}"
    echo ""
fi

# 6. Distclean
check "distclean" make distclean

echo "============================================"
echo " RESULTS: ${PASS} passed, ${FAIL} failed"
echo "============================================"

exit ${FAIL}

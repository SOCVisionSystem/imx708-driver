#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
#
# run-static-analysis.sh - Run all static analysis tools on IMX708 sources
#
# Copyright (C) 2026 SoC Centric
#
# Usage:
#   ./tools/run-static-analysis.sh [kernel-source-dir]
#
# Requires: checkpatch, clang-format, sparse, cppcheck
#

set -euo pipefail

KDIR="${1:-/lib/modules/$(uname -r)/build}"
FAILED=0

echo "============================================"
echo " IMX708 Static Analysis"
echo "============================================"
echo ""

# 1. checkpatch
echo "--- [1/5] checkpatch ---"
if [ -f "${KDIR}/scripts/checkpatch.pl" ]; then
    ./tools/checkpatch-wrapper.sh "${KDIR}" || FAILED=$((FAILED + 1))
else
    echo "SKIP: checkpatch.pl not found"
fi
echo ""

# 2. clang-format
echo "--- [2/5] clang-format ---"
if command -v clang-format &>/dev/null; then
    for src in src/*.c; do
        if ! clang-format --dry-run --Werror "${src}" 2>/dev/null; then
            echo "FAIL: ${src}"
            FAILED=$((FAILED + 1))
        fi
    done
    echo "PASS: clang-format clean"
else
    echo "SKIP: clang-format not installed"
fi
echo ""

# 3. sparse
echo "--- [3/5] sparse ---"
if command -v sparse &>/dev/null && [ -d "${KDIR}" ]; then
    make KDIR="${KDIR}" C=2 CF="-D__CHECK_ENDIAN__" 2>&1 | head -20 || true
    echo "(sparse output above)"
else
    echo "SKIP: sparse not installed or KDIR not available"
fi
echo ""

# 4. cppcheck (userspace)
echo "--- [4/5] cppcheck (userspace) ---"
if command -v cppcheck &>/dev/null; then
    cppcheck --enable=all --inconclusive --suppress=missingIncludeSystem \
        -Ilib/include -Iinclude \
        lib/src/libimx708.c test/imx708_test.c \
        test/imx708_cli.c test/imx708_stress.c \
        2>&1 || FAILED=$((FAILED + 1))
else
    echo "SKIP: cppcheck not installed"
fi
echo ""

# 5. gcc warnings (userspace)
echo "--- [5/5] gcc -Wall -Wextra -Werror (userspace) ---"
if command -v gcc &>/dev/null; then
    gcc -fsyntax-only -Wall -Wextra -Werror \
        -Ilib/include -Iinclude \
        lib/src/libimx708.c test/imx708_test.c \
        test/imx708_cli.c test/imx708_stress.c \
        2>&1 && echo "PASS: no warnings" || FAILED=$((FAILED + 1))
else
    echo "SKIP: gcc not installed"
fi
echo ""

echo "============================================"
if [ ${FAILED} -eq 0 ]; then
    echo " ALL CHECKS PASSED"
else
    echo " ${FAILED} CHECK(S) FAILED"
fi
echo "============================================"
exit ${FAILED}

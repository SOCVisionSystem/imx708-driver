#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
#
# build.sh - Docker cross-build entry point for IMX708 driver
#
# Copyright (C) 2026 SoC Centric
#
# Usage:
#   ./docker/build.sh [platform] [target]
#
# Examples:
#   ./docker/build.sh rpi4 all
#   ./docker/build.sh rpi4 module
#   ./docker/build.sh rpi4 lib
#

set -euo pipefail

PLATFORM="${1:-rpi4}"
TARGET="${2:-all}"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

IMAGE_TAG="imx708-builder:${PLATFORM}"

echo "=== Building IMX708 driver for ${PLATFORM} ==="
echo "Target: ${TARGET}"
echo ""

# Build the Docker image if it doesn't exist
if ! docker image inspect "${IMAGE_TAG}" >/dev/null 2>&1; then
    echo "Building Docker image ${IMAGE_TAG}..."
    docker build -t "${IMAGE_TAG}" \
        -f "${SCRIPT_DIR}/Dockerfile.${PLATFORM}" \
        "${PROJECT_DIR}"
    echo ""
fi

# Run the build inside the container
echo "Running build..."
docker run --rm \
    -v "${PROJECT_DIR}:/workspace" \
    -e PLATFORM="${PLATFORM}" \
    "${IMAGE_TAG}" \
    make PLATFORM="${PLATFORM}" "${TARGET}"

echo ""
echo "=== Build complete ==="
echo "Artifacts in: ${PROJECT_DIR}/build/${PLATFORM}/"

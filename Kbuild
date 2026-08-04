# SPDX-License-Identifier: GPL-2.0-only
#
# Kbuild - kernel module build description for imx708
#
# Copyright (C) 2026 SoC Centric
#
# This file is read by the kernel build system when building out-of-tree
# modules. It declares the module object and its component sources.
#

obj-m += imx708.o

imx708-y := src/imx708_main.o \
            src/imx708_platform.o \
            src/imx708_chardev.o \
            src/imx708_sysfs.o \
            src/imx708_debugfs.o \
            src/imx708_irq.o \
            src/imx708_pm.o \
            src/imx708_trace.o

# Fault injection lives in src/imx708_debugfs.c behind
# CONFIG_IMX708_FAULT_INJECT; there is no separate object file for it.

# DRV_VERSION is normally passed in by the top-level Makefile. Fall back to
# the VERSION file so that a bare "make -C <kdir> M=$PWD modules" still
# produces a sensible version string instead of an empty one.
DRV_VERSION ?= $(shell cat $(src)/VERSION 2>/dev/null || echo 0.0.0)

# Include paths for the module
ccflags-y := -I$(src)/include \
             -DDRV_VERSION=\"$(DRV_VERSION)\" \
             -Werror

# Trace header needs special include path
CFLAGS_src/imx708_trace.o := -I$(src)/include -I$(src)/src

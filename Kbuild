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

imx708-$(CONFIG_IMX708_FAULT_INJECT) += src/imx708_fault.o

# Include paths for the module
ccflags-y := -I$(src)/include \
             -DDRV_VERSION=\"$(DRV_VERSION)\" \
             -Werror

# Trace header needs special include path
CFLAGS_src/imx708_trace.o := -I$(src)/include -I$(src)/src

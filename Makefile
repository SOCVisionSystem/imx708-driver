# SPDX-License-Identifier: GPL-2.0-only
#
# Makefile - IMX708 Linux kernel driver + userspace library + tools
#
# Copyright (C) 2026 SoC Centric
#
# Builds:
#   - imx708.ko  — Linux kernel module (V4L2 sub-device)
#   - libimx708  — Userspace C library
#   - test apps  — imx708_test, imx708_cli, imx708_stress, imx708_capture
#
# Usage:
#   make              # build kernel module + lib + tests
#   make module       # kernel module only
#   make lib          # userspace library only
#   make test         # test applications only
#   make install      # install on target system
#   make clean        # clean build artifacts
#

DRV_NAME    := imx708
DRV_VERSION := $(shell cat VERSION 2>/dev/null || echo "0.1.0")

PLATFORM  ?= native
DESTDIR   ?= /
PREFIX    ?= /usr/local
JOBS      ?= $(shell nproc 2>/dev/null || echo 4)

# --- Platform detection ---
ifeq ($(PLATFORM),rpi4)
  ARCH            ?= arm64
  CROSS_COMPILE   ?= aarch64-linux-gnu-
  KERNEL_SRC      ?= $(HOME)/pi-kernel
  KDIR            ?= $(KERNEL_SRC)
else ifeq ($(PLATFORM),native)
  ARCH            ?= $(shell uname -m | sed 's/x86_64/x86_64/;s/aarch64/arm64/')
  CROSS_COMPILE   ?=
  KERNEL_SRC      ?= /lib/modules/$(shell uname -r)/build
  KDIR            ?= $(KERNEL_SRC)
else
  $(error Unsupported PLATFORM "$(PLATFORM)". Supported: native, rpi4)
endif

export ARCH CROSS_COMPILE KDIR

BUILD_DIR     := build/$(PLATFORM)
MODULE_DIR    := $(BUILD_DIR)/module
LIB_BUILD_DIR := $(BUILD_DIR)/lib
TEST_BUILD_DIR := $(BUILD_DIR)/test

CC      ?= $(CROSS_COMPILE)gcc
LD      ?= $(CROSS_COMPILE)ld
AR      ?= $(CROSS_COMPILE)ar
CFLAGS  ?= -O2 -Wall -Wextra -Werror
LDFLAGS ?=

.PHONY: all module module_clean lib lib_clean test test_clean \
        install module_install lib_install test_install \
        clean distclean help

# "clean" is deliberately NOT a prerequisite here: make does not order
# prerequisites, so under -j it could run concurrently with the build.
all: module lib test

# --- Kernel module (out-of-tree build) ---
module:
	@mkdir -p $(MODULE_DIR)
	$(MAKE) -C $(KDIR) M=$(CURDIR) DRV_VERSION=$(DRV_VERSION) modules
	# Move build artifacts to out-of-tree build directory
	@mv -f *.ko *.o *.mod* Module.symvers modules.order $(MODULE_DIR)/ 2>/dev/null || true
	@rm -f .*.cmd .*.o.d 2>/dev/null || true
	@echo "Module built: $(MODULE_DIR)/imx708.ko"

module_clean:
	rm -rf $(MODULE_DIR)
	rm -f *.ko *.o *.mod* Module.symvers modules.order .*.cmd .*.o.d
	rm -f src/*.o src/*.mod* src/.*.cmd

# --- Userspace library ---
LIB_SRC  := lib/src/libimx708.c
LIB_OBJ  := $(LIB_BUILD_DIR)/libimx708.o
LIB_A    := $(LIB_BUILD_DIR)/libimx708.a
LIB_SO   := $(LIB_BUILD_DIR)/libimx708.so
LIB_SO_MAJ := $(LIB_SO).0
LIB_SO_FULL := $(LIB_SO_MAJ).$(DRV_VERSION)

LIB_CFLAGS := -Ilib/include -Iinclude -fPIC -fvisibility=hidden \
              $(CFLAGS) -D_GNU_SOURCE
LIB_LDFLAGS := $(LDFLAGS) -shared -Wl,-soname,libimx708.so.0 \
               -Wl,--version-script=lib/libimx708.version

LIB_PC := $(LIB_BUILD_DIR)/libimx708.pc

lib: $(LIB_SO_FULL) $(LIB_A) $(LIB_PC)

# lib/libimx708.pc is generated, not checked in; "make install" used to
# reference a file that never existed.
$(LIB_PC): lib/libimx708.pc.in VERSION
	@mkdir -p $(LIB_BUILD_DIR)
	sed -e 's|@PREFIX@|$(PREFIX)|g' \
	    -e 's|@DRV_VERSION@|$(DRV_VERSION)|g' $< > $@

$(LIB_OBJ): $(LIB_SRC) lib/include/libimx708.h include/imx708_uapi.h
	@mkdir -p $(LIB_BUILD_DIR)
	$(CC) $(LIB_CFLAGS) -c -o $@ $<

$(LIB_SO_FULL): $(LIB_OBJ) lib/libimx708.version
	$(CC) $(LIB_LDFLAGS) -o $@ $<
	ln -sf libimx708.so.0.$(DRV_VERSION) $(LIB_SO_MAJ)
	ln -sf libimx708.so.0.$(DRV_VERSION) $(LIB_SO)

$(LIB_A): $(LIB_OBJ)
	$(AR) rcs $@ $<

lib_clean:
	rm -rf $(LIB_BUILD_DIR)

# --- Test applications ---
TEST_CFLAGS := -Ilib/include -Iinclude $(CFLAGS) -D_GNU_SOURCE
TEST_LDFLAGS := $(LDFLAGS) -L$(LIB_BUILD_DIR) -limx708 -lpthread \
                -Wl,-rpath,$(abspath $(LIB_BUILD_DIR))

TEST_BINS := $(TEST_BUILD_DIR)/imx708_test \
             $(TEST_BUILD_DIR)/imx708_cli \
             $(TEST_BUILD_DIR)/imx708_stress \
             $(TEST_BUILD_DIR)/imx708_capture

test: lib $(TEST_BINS)

$(TEST_BUILD_DIR)/imx708_test: test/imx708_test.c $(LIB_SO_FULL)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(TEST_CFLAGS) -o $@ $< $(TEST_LDFLAGS)

$(TEST_BUILD_DIR)/imx708_cli: test/imx708_cli.c $(LIB_SO_FULL)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(TEST_CFLAGS) -o $@ $< $(TEST_LDFLAGS)

$(TEST_BUILD_DIR)/imx708_stress: test/imx708_stress.c $(LIB_SO_FULL)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(TEST_CFLAGS) -o $@ $< $(TEST_LDFLAGS)

$(TEST_BUILD_DIR)/imx708_capture: test/imx708_capture.c $(LIB_SO_FULL)
	@mkdir -p $(TEST_BUILD_DIR)
	$(CC) $(TEST_CFLAGS) -o $@ $< $(TEST_LDFLAGS)

test_clean:
	rm -rf $(TEST_BUILD_DIR)

# --- Install ---
install: module_install lib_install test_install

# KVER defaults to the running kernel, which is wrong for a cross build;
# pass KVER=<target release> when PLATFORM=rpi4.
KVER ?= $(shell uname -r)

module_install: module
	install -d $(DESTDIR)/lib/modules/$(KVER)/extra
	install -m 644 $(MODULE_DIR)/imx708.ko $(DESTDIR)/lib/modules/$(KVER)/extra/
	depmod -a $(KVER) 2>/dev/null || true

lib_install: lib
	install -d $(DESTDIR)$(PREFIX)/lib
	install -m 755 $(LIB_SO_FULL) $(DESTDIR)$(PREFIX)/lib/
	ln -sf libimx708.so.0.$(DRV_VERSION) $(DESTDIR)$(PREFIX)/lib/libimx708.so.0
	ln -sf libimx708.so.0.$(DRV_VERSION) $(DESTDIR)$(PREFIX)/lib/libimx708.so
	install -m 644 $(LIB_A) $(DESTDIR)$(PREFIX)/lib/
	install -d $(DESTDIR)$(PREFIX)/include
	install -m 644 lib/include/libimx708.h $(DESTDIR)$(PREFIX)/include/
	install -d $(DESTDIR)$(PREFIX)/lib/pkgconfig
	install -m 644 $(LIB_PC) $(DESTDIR)$(PREFIX)/lib/pkgconfig/

test_install: test
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TEST_BUILD_DIR)/imx708_test $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(TEST_BUILD_DIR)/imx708_cli $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(TEST_BUILD_DIR)/imx708_stress $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(TEST_BUILD_DIR)/imx708_capture $(DESTDIR)$(PREFIX)/bin/

# --- Clean ---
clean: module_clean lib_clean test_clean
	rm -f *.ko *.o *.mod* Module.symvers modules.order .*.cmd .*.o.d
	rm -rf build/

distclean: clean

# --- Help ---
help:
	@echo "IMX708 Driver — Linux kernel module + C library + tools"
	@echo "Version: $(DRV_VERSION)"
	@echo ""
	@echo "Targets:"
	@echo "  all              Build kernel module + lib + test (default)"
	@echo "  module           Build kernel module only"
	@echo "  lib              Build userspace library"
	@echo "  test             Build test applications"
	@echo "  install          Install everything"
	@echo "  clean            Clean build artifacts"
	@echo ""
	@echo "Parameters:"
	@echo "  PLATFORM=native|rpi4  Target platform"
	@echo "  KERNEL_SRC=/path      Kernel source tree"
	@echo "  DESTDIR=/staging      Install root"
	@echo "  PREFIX=/usr           Install prefix"

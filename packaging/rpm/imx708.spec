# SPDX-License-Identifier: GPL-2.0-only
#
# imx708.spec - RPM spec for IMX708 sensor driver
#
# Copyright (C) 2026 SoC Centric
#

%define name imx708
%define version 0.1.0
%define release 1
%define _prefix /usr

Name: %{name}
Version: %{version}
Release: %{release}
Summary: Sony IMX708 camera sensor driver
License: GPL-2.0-only
Group: System Environment/Kernel
URL: https://github.com/soccentric/imx708
BuildRequires: kernel-devel, gcc, make
Requires: kernel

%description
Kernel module and userspace library for the Sony IMX708 12MP
MIPI CSI-2 camera sensor used in Raspberry Pi Camera Module 3.

%package devel
Summary: Development files for IMX708 sensor library
Group: Development/Libraries
Requires: %{name} = %{version}-%{release}

%description devel
Headers, static library, and pkg-config file for developing
applications that use the IMX708 camera sensor.

%package tools
Summary: Test and diagnostic tools for IMX708 sensor
Group: Development/Tools
Requires: %{name} = %{version}-%{release}

%description tools
Test suite, interactive CLI, and stress test tool for the
IMX708 camera sensor driver.

%prep
%setup -q

%build
make all

%install
make install DESTDIR=%{buildroot} PREFIX=%{_prefix}

%post
/sbin/depmod -a

%postun
/sbin/depmod -a

%files
%{_prefix}/lib/modules/*/extra/imx708.ko*
%{_prefix}/lib/libimx708.so.*
%{_prefix}/lib/udev/rules.d/*

%files devel
%{_prefix}/include/libimx708.h
%{_prefix}/lib/libimx708.a
%{_prefix}/lib/libimx708.so
%{_prefix}/lib/pkgconfig/libimx708.pc

%files tools
%{_prefix}/bin/imx708_test
%{_prefix}/bin/imx708_cli
%{_prefix}/bin/imx708_stress

%changelog
* Mon Jan 01 2026 Sandesh <sandesh@soccentric.com> - 0.1.0-1
- Initial release

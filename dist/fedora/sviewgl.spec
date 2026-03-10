Name: sviewgl
Version: _VERSION_
Release: 1%{?dist}
Summary: Lightweight hardware-accelerated image viewer

License: GPLv2+
URL: https://github.com/reybits/simple-viewer-gl
Source0: %{name}-%{version}.tar.gz

BuildRequires: gcc-c++
BuildRequires: make
BuildRequires: cmake >= 3.22
BuildRequires: pkgconfig
BuildRequires: mesa-libGL-devel
BuildRequires: glfw-devel
BuildRequires: libpng-devel
BuildRequires: libjpeg-turbo-devel
BuildRequires: zlib-devel
BuildRequires: libexif-devel
BuildRequires: lcms2-devel
BuildRequires: openjpeg2-devel
BuildRequires: giflib-devel
BuildRequires: libtiff-devel
BuildRequires: libwebp-devel
BuildRequires: openexr-devel
BuildRequires: libcurl-devel

%description
Simple Viewer GL is a fast, minimalist image viewer using OpenGL for
hardware-accelerated rendering. It features vi-like keybindings and supports
25+ image formats including PNG, JPEG, WebP, TIFF, PSD, XCF, SVG, OpenEXR,
JPEG 2000, GIF (animated), ICO, BMP, TGA, DDS, PVR, and more.

%prep
%setup -q

%build
%{__make} %{?_smp_mflags} release

%install
%{__make} install DESTDIR=%{buildroot} PREFIX=%{_prefix}

%files
%license Copying.txt
%{_bindir}/sviewgl
%{_datadir}/applications/sviewgl.desktop
%{_datadir}/icons/hicolor/*/apps/sviewgl.png

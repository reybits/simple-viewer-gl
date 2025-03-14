Prefix: %{_usr}
Name: sviewgl
Version: _VERSION_
Release: 1%{?dist}
Summary: Simple Viewer GL - simple and tiny image viewer based on OpenGL

Group: LOR/stuff
License: GPLv2
URL: https://github.com/reybits/simple-viewer-gl.git
Source0: %{name}-%{version}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires: gcc-c++, make, cmake
BuildRequires: mesa-libGL-devel, glfw-devel, freetype-devel, libpng-devel, libjpeg-turbo-devel, libtiff-devel, giflib-devel, lcms2-devel, libwebp-devel, libexif-devel, imlib2-devel, zlib-devel, ilmbase-devel, OpenEXR-devel

%description
Simple Viewer GL - simple and tiny image viewer based on OpenGL

%prep
%setup -q

%build
%{__make} %{?_smp_mflags} CXXFLAGS="${RPM_OPT_FLAGS}" LFLAGS="${RPM_LD_FLAGS}" release

%install
rm -rf $RPM_BUILD_ROOT
%{__make} install DESTDIR=${RPM_BUILD_ROOT} PREFIX=/usr

%clean
rm -rf ${RPM_BUILD_ROOT}

%post
/sbin/ldconfig

%postun
/sbin/ldconfig

%files
/usr/bin/sviewgl

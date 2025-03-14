# Simple Viewer GL

**Simple Viewer GL** is a simple and tiny image viewer based on OpenGL.

![Simple Viewer GL](https://github.com/andreyugolnik/simple-viewer-gl/blob/master/res/Featured-1024x500.png)

***

![GitHub Release](https://img.shields.io/github/v/release/andreyugolnik/simple-viewer-gl)
![GitHub commits since latest release (development)](https://img.shields.io/github/commits-since/andreyugolnik/simple-viewer-gl/latest/development)

master | development
:----: | :----:
[![Build status: master](https://ci.appveyor.com/api/projects/status/55qlv1c7ca5vp7y4/branch/master?svg=true)](https://ci.appveyor.com/project/andreyugolnik/simple-viewer-gl/branch/master "Branch: master") ![GitHub last commit (master)](https://img.shields.io/github/last-commit/andreyugolnik/simple-viewer-gl/master) | [![Build status: development](https://ci.appveyor.com/api/projects/status/55qlv1c7ca5vp7y4/branch/development?svg=true)](https://ci.appveyor.com/project/andreyugolnik/simple-viewer-gl/branch/development "Branch: development") ![GitHub last commit (development)](https://img.shields.io/github/last-commit/andreyugolnik/simple-viewer-gl/development)

The primary goal of **Simple Viewer GL** is to provide a fast, efficient image viewer with only the essential features required for quick image browsing. It includes *vi*-like key bindings and integrates seamlessly with tiling window managers such as *ion3*/*notion*, *i3wm*, *dwm*, *xmonad*, *hyprland*, *sway*, and others.

Supported formats include **PNG**, **JPEG**, **JPEG 2000**, **PSD** (Adobe Photoshop), **AI** (Adobe Illustrator), **EPS**, **XCF** (GIMP), **GIF**, **SVG**, **TIFF**, **TARGA**, **ICO**, **ICNS** (Apple Icon Image), **BMP**, **PNM**, **DDS**, **XWD**, **SCR** (ZX-Spectrum screen), **XPM**, **WebP**, **OpenEXR**, and many more.

***
## Screenshots

![Simple Viewer GL on macOS with Pixel Info](https://raw.githubusercontent.com/reybits/simple-viewer-gl/master/res/Screenshot-PixelInfo.png "Simple Viewer GL")
![Simple Viewer GL on macOS with EXIF](https://raw.githubusercontent.com/reybits/simple-viewer-gl/master/res/Screenshot-EXIF.png "Simple Viewer GL")

***
## Features

- [x] Lightweight and fast: utilize hardware-accelerated video card;
- [x] Support embedded ICC profiles;
- [x] GIF animation support;
- [x] GIMP XCF support;
- [x] Adobe PSD format support;
- [x] Adobe AI, EPS formats preview support;
- [x] SVG format support;
- [x] Exif support;
- [x] Very simple interface;
- [x] Suitable for default image viewer for the desktops and laptops;
- [x] Minimal lib dependency: look library requirements below;
- [x] Desktop independent: doesn't require any specific desktop environment;
- [x] Open source, licensed under GNU GPL;

***
## Usage

**Simple Viewer GL** offers two modes: Image Viewer and Image Info mode. By default, it opens in Image Viewer mode, displaying only the current image. In Image Info mode, additional features such as pixel information and rectangular selection are available, making it useful for quickly reviewing image details and metadata.

***
## Key bindings

Hotkey                    | Action
:------------------------ | :--------------------------------
`<esc>`                   | exit
`<space>`                 | next image
`<backspace>`             | previous image
`<home>`                  | first file in list
`<end>`                   | last file in list
`<+>` / `<->`             | scale image
`<1>`...`<0>`             | set scale from 100% to 1000%
`<enter>`                 | switch fullscreen / windowed mode
`<h>` / `<l>`             | pan left / right in pixels
`<j>` / `<k>`             | pan down / up in pixels
`<shift>` + `<h>` / `<l>` | pan left / right in percents
`<shift>` + `<j>` / `<k>` | pan down / up in percents
`<del>`                   | toggle deletion mark
`<ctrl>` + `<del>`        | delete marked images from disk
`<r>`                     | rotate clockwise
`<shift>` + `<r>`         | rotate counterclockwise
`<pgup>` / `<bgdn>`       | previous /next subimage
`<s>`                     | fit image to window
`<shift>` + `<s>`         | toggle 'keep scale' on image load
`<c>`                     | hide / show chequerboard
`<i>`                     | hide / show on-screen info
`<e>`                     | hide / show exif
`<p>`                     | hide / show pixel info
`<b>`                     | hide / show border around image
`<g>`                     | hide / show image grid
`<?>`                     | hide / show keybindings popup

***
## Download and build from sources

You can browse the source code repository on GitHub or get a copy using git with the following command:

```sh
git clone https://github.com/reybits/simple-viewer-gl.git
cd simple-viewer-gl
make release
```
> On success **sviewgl** binary produced in current directory. Just symlink it `ln -s /path/to/sviewgl /usr/bin/sviewgl` or copy it `cp /path/to/sviewgl /usr/bin/`.

[Slackbuild by](https://github.com/saahriktu/saahriktu-slackbuilds/tree/master/simple-viewer-gl) [saahriktu](https://www.linux.org.ru/people/saahriktu/profile).
[Gentoo ebuild by](https://gogs.lumi.pw/mike/portage/src/master/media-gfx/simpleviewer-gl) [imul](https://www.linux.org.ru/people/imul/profile)

***
## Make DEB package

Update and install required dependencies:
```sh
sudo apt-get update
sudo apt-get install g++ make build-essential debhelper cmake pkg-config libgl1-mesa-dev
sudo apt-get install libxrandr-dev libxcursor-dev libfreetype6-dev libjpeg-dev
sudo apt-get install libtiff-dev libgif-dev liblcms2-dev libimlib2-dev libwebp-dev
sudo apt-get install libglfw3-dev libexif-dev libilmbase-dev libopenexr-dev
```

Clone and make DEB:
```sh
git clone https://github.com/reybits/simple-viewer-gl.git
cd simple-viewer-gl
make deb
```

> You can purge installed packages with `apt-get purge PACKAGE_NAME && apt-get autoremove`

***
## Make RPM package

Update and install required dependencies:
```sh
sudo dnf install gcc-c++ make cmake mesa-libGL-devel glfw-devel freetype-devel
sudo dnf install libpng-devel libjpeg-turbo-devel libtiff-devel giflib-devel lcms2-devel
sudo dnf install libwebp-devel libexif-devel imlib2-devel zlib-devel ilmbase-devel OpenEXR-devel
```

Clone and make source for RPM:
```sh
git clone https://github.com/reybits/simple-viewer-gl.git
cd simple-viewer-gl
sudo make rpm
```

***
## Dependencies

Name     | Debian package       | Description / Notes
:------- | :------------------- | :------------------------------------------------------
Make     | make                 | Utility for directing compilation.
Cmake    | cmake                | Cross-platform, open-source make system.
Mesa     | libgl1-mesa-dev      | Transitional dummy package.
GLFW3    | libglfw3-dev         | Portable library for OpenGL, window and input.
Zlib     | zlib1g-dev           | Compression support.
PNG      | libpng-dev           | PNG format support.
JPEG     | libjpeg-turbo8-dev   | JPEG format support.
TIFF     | libtiff-dev          | (optional) TIFF format support.
OpenJPEG | libopenjp2-7-dev     | (optional) JPEG 2000 support.
curl     | libcurl4-openssl-dev | (optional) Support loading via http/https/ftp protocols.

> Copyright © 2008-2024 Andrey A. Ugolnik. All Rights Reserved.
> https://www.ugolnik.info
> andrey@ugolnik.info
>
> Icon was created by
> Iryna Poliakova (Iryna.poliakova@icloud.com).

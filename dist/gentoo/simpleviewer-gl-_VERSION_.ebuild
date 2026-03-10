# Copyright 1999-2026 Gentoo Authors
# Distributed under the terms of the GNU General Public License v2

EAPI=8

inherit cmake

DESCRIPTION="Lightweight hardware-accelerated image viewer using OpenGL"
HOMEPAGE="https://github.com/reybits/simple-viewer-gl"
EGIT_REPO_URI="https://github.com/reybits/simple-viewer-gl.git"
if [[ ${PV} == 9999 ]]; then
	inherit git-r3
	EGIT_BRANCH="master"
else
	SRC_URI="https://github.com/reybits/simple-viewer-gl/archive/v${PV}.tar.gz -> ${P}.tar.gz"
	S="${WORKDIR}/simple-viewer-gl-${PV}"
	KEYWORDS="~amd64 ~arm64 ~x86"
fi
LICENSE="GPL-2"
SLOT="0"
IUSE="+lcms +exif +jpeg2k +gif +tiff +webp +exr +curl"

DEPEND="
	media-libs/glfw
	virtual/opengl
	virtual/jpeg
	media-libs/libpng
	sys-libs/zlib
	lcms? ( media-libs/lcms:2 )
	exif? ( media-libs/libexif )
	jpeg2k? ( media-libs/openjpeg:2 )
	gif? ( media-libs/giflib )
	tiff? ( media-libs/tiff )
	webp? ( media-libs/libwebp )
	exr? ( media-libs/openexr:= )
	curl? ( net-misc/curl )
"

RDEPEND="${DEPEND}"
BDEPEND="virtual/pkgconfig"

src_configure() {
	VER=($(awk -F= '/^VER_/{ print $2 }' <Makefile))
	local mycmakeargs=(
		-DCMAKE_BUILD_TYPE=Release
		-DAPP_VERSION_MAJOR:STRING=${VER[0]}
		-DAPP_VERSION_MINOR:STRING=${VER[1]}
		-DAPP_VERSION_RELEASE:STRING=${VER[2]}
		-DDISABLE_LCMS2_SUPPORT=$(usex lcms 0 1)
		-DDISABLE_EXIF_SUPPORT=$(usex exif 0 1)
		-DDISABLE_JPEG2000_SUPPORT=$(usex jpeg2k 0 1)
		-DDISABLE_GIF_SUPPORT=$(usex gif 0 1)
		-DDISABLE_TIFF_SUPPORT=$(usex tiff 0 1)
		-DDISABLE_WEBP_SUPPORT=$(usex webp 0 1)
		-DDISABLE_OPENEXR_SUPPORT=$(usex exr 0 1)
		-DDISABLE_CURL_SUPPORT=$(usex curl 0 1)
	)
	cmake_src_configure
}

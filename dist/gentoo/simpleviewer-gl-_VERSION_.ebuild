# Copyright 1999-2017 Gentoo Foundation
# Distributed under the terms of the GNU General Public License v2
# $Header: $

EAPI=6

inherit cmake-utils git-r3

DESCRIPTION="Small and simple image viewer for Linux."
HOMEPAGE="https://www.ugolnik.info/?p=1302"
EGIT_REPO_URI="https://github.com/reybits/simple-viewer-gl.git"
if [[ ${PV} == 9999 ]]; then
	EGIT_BRANCH="development"
else
	TAG="v${PV}"
fi
LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~x86 ~amd64"
IUSE="+lcms +exif +jpeg2k +gif +tiff +webp +exr +curl -imlib2"

DEPEND="
	media-libs/glfw
	media-libs/freetype:2
	virtual/opengl
	virtual/jpeg
	media-libs/libpng
	lcms? ( media-libs/lcms )
	exif? ( media-libs/libexif )
	jpeg2k? ( media-libs/openjpeg )
	gif? ( media-libs/giflib )
	tiff? ( media-libs/tiff )
	webp? ( media-libs/libwebp )
	exr? ( media-libs/openexr
	       media-libs/ilmbase )
	curl? ( net-misc/curl )
	imlib2? ( media-libs/imlib2[X,jpeg,gif,png,tiff] )
"

RDEPEND="${DEPEND}"

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
		-DDISABLE_IMLIB2_SUPPORT=$(usex imlib2 0 1)
	)
	cmake-utils_src_configure
}

src_install() {
	dobin "${BUILD_DIR}"/sviewgl
	dodoc config.example
}

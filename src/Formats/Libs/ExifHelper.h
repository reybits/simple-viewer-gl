/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Common/ImageInfo.h"

#include <cstdint>

namespace exif {

// Parse EXIF binary data and extract all tags into exifList,
// grouped by IFD → ExifCategory. Also extracts orientation.
void extractAll(const uint8_t* data, unsigned size, sImageInfo::ExifList& exifList, uint16_t& orientation);

} // namespace exif

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

// Read the EXIF orientation tag (IFD0, 0x0112) from a raw EXIF/TIFF block.
// Dependency-free (no libexif) and always available; returns Orientation::Normal
// when absent or invalid. Single source of orientation for every format.
sImageInfo::Orientation readOrientation(const uint8_t* data, unsigned size);

// Parse EXIF binary data and extract all tags into exifList,
// grouped by IFD -> ExifCategory.
void extractAll(const uint8_t* data, unsigned size, sImageInfo::ExifList& exifList);

} // namespace exif

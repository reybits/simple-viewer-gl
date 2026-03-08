/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "NotAvailable.h"
#include "Common/BitmapDescription.h"
#include "img-na.c"

#include <cstring>

cNotAvailable::cNotAvailable()
    : cFormat(nullptr)
{
}

bool cNotAvailable::LoadImpl(const char* /*filename*/, sBitmapDescription& desc)
{
    desc.format = imgNa.bytes_per_pixel == 3
        ? ePixelFormat::RGB
        : ePixelFormat::RGBA;
    desc.width = imgNa.width;
    desc.height = imgNa.height;
    desc.bpp = imgNa.bytes_per_pixel * 8;
    desc.bppImage = 0;
    desc.pitch = desc.width * imgNa.bytes_per_pixel;

    const auto size = desc.pitch * desc.height;
    desc.bitmap.resize(size);
    ::memcpy(desc.bitmap.data(), imgNa.pixel_data, size);

    desc.formatName = "n/a";

    return true;
}

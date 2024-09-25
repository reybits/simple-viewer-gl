/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "notavailable.h"
#include "common/bitmap_description.h"
#include "img-na.c"

#include <cstring>

cNotAvailable::cNotAvailable()
    : cFormat(nullptr)
{
}

cNotAvailable::~cNotAvailable()
{
}

bool cNotAvailable::LoadImpl(const char* /*filename*/, sBitmapDescription& desc)
{
    desc.format   = imgNa.bytes_per_pixel == 3 ? GL_RGB : GL_RGBA;
    desc.width    = imgNa.width;
    desc.height   = imgNa.height;
    desc.bpp      = imgNa.bytes_per_pixel * 8;
    desc.bppImage = 0;
    desc.pitch    = desc.width * imgNa.bytes_per_pixel;

    const unsigned size = desc.pitch * desc.height;
    desc.bitmap.resize(size);
    ::memcpy(&desc.bitmap[0], imgNa.pixel_data, size);

    m_formatName = "n/a";

    return true;
}

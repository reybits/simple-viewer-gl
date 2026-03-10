/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "NotAvailable.h"
#include "Common/ChunkData.h"
#include "Common/ImageInfo.h"
#include "Assets/img-na.c"

#include <cstring>

cNotAvailable::cNotAvailable()
    : cFormat(nullptr)
{
}

bool cNotAvailable::LoadImpl(const char* /*filename*/, sChunkData& chunk, sImageInfo& info)
{
    chunk.format = imgNa.bytes_per_pixel == 3
        ? ePixelFormat::RGB
        : ePixelFormat::RGBA;
    chunk.width = imgNa.width;
    chunk.height = imgNa.height;
    chunk.bpp = imgNa.bytes_per_pixel * 8;
    info.bppImage = 0;
    chunk.pitch = chunk.width * imgNa.bytes_per_pixel;

    const auto size = chunk.pitch * chunk.height;
    chunk.bitmap.resize(size);
    ::memcpy(chunk.bitmap.data(), imgNa.pixel_data, size);

    info.formatName = "n/a";

    return true;
}

/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#if defined(WEBP_SUPPORT)

#include "formatwebp.h"
#include "common/bitmap_description.h"
#include "common/file.h"

#include <cstdio>
#include <cstring>

#include <webp/decode.h>

cFormatWebP::cFormatWebP(iCallbacks* callbacks)
    : cFormat(callbacks)
{
}

cFormatWebP::~cFormatWebP()
{
}

bool cFormatWebP::isSupported(cFile& file, Buffer& buffer) const
{
#pragma pack(push, 1)
    struct WebPheader
    {
        uint8_t riff[4];
        uint32_t size;
        uint8_t webp[4];
    };
#pragma pack(pop)

    if (!readBuffer(file, buffer, sizeof(WebPheader)))
    {
        return false;
    }

    const uint8_t riff[4] = { 'R', 'I', 'F', 'F' };
    const uint8_t webp[4] = { 'W', 'E', 'B', 'P' };
    auto h = reinterpret_cast<const WebPheader*>(buffer.data());
    return h->size == file.getSize() - 8
        && !::memcmp(h->riff, riff, 4)
        && !::memcmp(h->webp, webp, 4);
}

bool cFormatWebP::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    cFile file;
    if (!file.open(filename))
    {
        return false;
    }

    Buffer buffer;
    buffer.resize(file.getSize());
    if (file.read(buffer.data(), file.getSize()) != file.getSize())
    {
        ::printf("(EE) Error loading WebP.\n");
        return false;
    }

    WebPBitstreamFeatures features;
    auto error = WebPGetFeatures(buffer.data(), buffer.size(), &features);
    if (error != VP8_STATUS_OK)
    {
        ::printf("(EE) Error loading WebP: %d.\n", error);
        return false;
    }

    desc.size = file.getSize();

    desc.images = 1;
    desc.current = 0;
    desc.width = features.width;
    desc.height = features.height;

    // ::printf("alpha: %d, animations: %d, format: %d\n"
    // , features.has_alpha, features.has_animation
    // , features.format);

    if (features.has_alpha)
    {
        desc.pitch = features.width * 4;
        desc.bpp = 32;
        desc.bppImage = 32;
        desc.format = ePixelFormat::RGBA;
        desc.bitmap.resize(desc.pitch * desc.height);
        signalBitmapAllocated();

        if (WebPDecodeRGBAInto(buffer.data(), buffer.size(), desc.bitmap.data(), desc.bitmap.size(), desc.pitch) == nullptr)
        {
            ::printf("(EE) Error decoding WebP.\n");
            return false;
        }
    }
    else
    {
        desc.pitch = features.width * 3;
        desc.bpp = 24;
        desc.bppImage = 24;
        desc.format = ePixelFormat::RGB;
        desc.bitmap.resize(desc.pitch * desc.height);
        signalBitmapAllocated();

        if (WebPDecodeRGBInto(buffer.data(), buffer.size(), desc.bitmap.data(), desc.bitmap.size(), desc.pitch) == nullptr)
        {
            ::printf("(EE) Error decoding WebP.\n");
            return false;
        }
    }

    m_formatName = "webp";

    return true;
}

#endif

/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#if defined(WEBP_SUPPORT)

#include "FormatWebP.h"
#include "Common/BitmapDescription.h"
#include "Common/File.h"
#include "Log/Log.h"

#include <cstring>
#include <webp/decode.h>
#include <webp/demux.h>

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
    if (!openFile(file, filename, desc))
    {
        return false;
    }

    Buffer buffer;
    buffer.resize(file.getSize());
    if (file.read(buffer.data(), file.getSize()) != file.getSize())
    {
        cLog::Error("Can't load WebP file.");
        return false;
    }

    WebPBitstreamFeatures features;
    auto error = WebPGetFeatures(buffer.data(), buffer.size(), &features);
    if (error != VP8_STATUS_OK)
    {
        cLog::Error("Can't load WebP file: {}.", static_cast<int>(error));
        return false;
    }

    desc.images = 1;
    desc.current = 0;
    desc.width = features.width;
    desc.height = features.height;

    if (features.has_alpha)
    {
        desc.bppImage = 32;
        setupBitmap(desc, desc.width, desc.height, 32, ePixelFormat::RGBA, "webp");

        if (WebPDecodeRGBAInto(buffer.data(), buffer.size(), desc.bitmap.data(), desc.bitmap.size(), desc.pitch) == nullptr)
        {
            cLog::Error("Can't decode WebP data.");
            return false;
        }
    }
    else
    {
        desc.bppImage = 24;
        setupBitmap(desc, desc.width, desc.height, 24, ePixelFormat::RGB, "webp");

        if (WebPDecodeRGBInto(buffer.data(), buffer.size(), desc.bitmap.data(), desc.bitmap.size(), desc.pitch) == nullptr)
        {
            cLog::Error("Can't decode WebP data.");
            return false;
        }
    }

    // Extract ICC profile via demux API
    WebPData webpData = { buffer.data(), buffer.size() };
    auto demux = WebPDemux(&webpData);
    if (demux != nullptr)
    {
        WebPChunkIterator chunkIter;
        if (WebPDemuxGetChunk(demux, "ICCP", 1, &chunkIter))
        {
            if (applyIccProfile(desc, chunkIter.chunk.bytes, static_cast<uint32_t>(chunkIter.chunk.size)))
            {
                desc.formatName = "webp/icc";
            }
            WebPDemuxReleaseChunkIterator(&chunkIter);
        }
        WebPDemuxDelete(demux);
    }

    return true;
}

#endif

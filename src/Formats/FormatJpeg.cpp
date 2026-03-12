/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatJpeg.h"
#include "Common/Callbacks.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
#include "Libs/ExifHelper.h"
#include "Log/Log.h"

#include <cstring>

bool cFormatJpeg::isSupported(cFile& file, Buffer& buffer) const
{
    if (readBuffer(file, buffer, 4) == false)
    {
        return false;
    }

    const auto h = reinterpret_cast<const uint8_t*>(buffer.data());

    constexpr struct
    {
        uint8_t four[4];
    } Heads[] = {
        { { 0xff, 0xd8, 0xff, 0xdb } },
        { { 0xff, 0xd8, 0xff, 0xe0 } },
        { { 0xff, 0xd8, 0xff, 0xed } },
        { { 0xff, 0xd8, 0xff, 0xe1 } },
        { { 0xff, 0xd8, 0xff, 0xe2 } },
        { { 0xff, 0xd8, 0xff, 0xee } },
        { { 0xff, 0xd8, 0xff, 0xfe } },
    };

    for (auto& header : Heads)
    {
        if (std::memcmp(h, header.four, sizeof(header)) == 0)
        {
            return true;
        }
    }

    return false;
}

bool cFormatJpeg::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (openFile(file, filename, info) == false)
    {
        return false;
    }

    const auto size = file.getSize();
    Buffer in(size);
    file.read(in.data(), size);

    auto progressCb = [this](float p) { updateProgress(p); };
    auto allocatedCb = [this]() { signalBitmapAllocated(); };
    auto imageInfoCb = [this]() { signalImageInfo(); };
    auto previewCb = [this, &chunk](cJpegDecoder::Bitmap&& thumb) {
        sPreviewData preview;
        preview.bitmap = std::move(thumb.data);
        preview.width = thumb.width;
        preview.height = thumb.height;
        preview.pitch = thumb.pitch;
        preview.bpp = thumb.bpp;
        preview.format = thumb.format;
        preview.fullImageWidth = chunk.width;
        preview.fullImageHeight = chunk.height;
        signalPreviewReady(std::move(preview));
    };
    auto result = m_decoder.decodeJpeg(in.data(), static_cast<uint32_t>(size), chunk, info, progressCb, allocatedCb, imageInfoCb, previewCb, m_stop);
    if (result.success == false)
    {
        return false;
    }

    // ICC LUT generated inside decodeJpeg() — applied on GPU during rendering.

    if (result.exifData.empty() == false)
    {
        exif::extractAll(result.exifData.data(), static_cast<unsigned>(result.exifData.size()),
                         info.exifList, info.exifOrientation);
    }

    return true;
}

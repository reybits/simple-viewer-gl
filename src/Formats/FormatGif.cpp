/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#if defined(GIF_SUPPORT)

#include "FormatGif.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
#include "Log/Log.h"

#include <cstring>
#include <memory>

void cFormatGif::GifDeleter::operator()(GifFileType* gifFile)
{
#if GIFLIB_MAJOR >= 5
    int errorCode = 0;
    auto result = DGifCloseFile(gifFile, &errorCode);
    (void)errorCode;
#else
    auto result = DGifCloseFile(gifFile);
#endif

    (void)result;
}

namespace
{
    GifFileType* OpenFile(const char* path)
    {
#if GIFLIB_MAJOR >= 5
        int errorCode = 0;
        auto gifFile = DGifOpenFileName(path, &errorCode);
        (void)errorCode;
#else
        auto gifFile = DGifOpenFileName(path);
#endif

        return gifFile;
    }

    const char* GetError(GifFileType* gif)
    {
#if GIFLIB_MAJOR >= 5
        if (gif != nullptr)
        {
            return GifErrorString(gif->Error);
        }
#endif
        return "n/a";
    }

    void putPixel(sChunkData& chunk, uint32_t pos, const GifColorType* color, bool transparent)
    {
        if (transparent == false)
        {
            chunk.bitmap[pos + 0] = color->Red;
            chunk.bitmap[pos + 1] = color->Green;
            chunk.bitmap[pos + 2] = color->Blue;
            chunk.bitmap[pos + 3] = 255;
        }
    }

    void putRow(sChunkData& chunk, uint32_t row, uint32_t width, const SavedImage& image, const ColorMapObject* cmap, uint32_t transparentIdx)
    {
        for (uint32_t x = 0; x < width; x++)
        {
            const uint32_t idx = image.RasterBits[row * width + x];
            const uint32_t pos = (row + image.ImageDesc.Top) * chunk.pitch + (x + image.ImageDesc.Left) * 4;
            putPixel(chunk, pos, &cmap->Colors[idx], transparentIdx == idx);
        }
    }
} // namespace

bool cFormatGif::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, 6))
    {
        return false;
    }

    const auto h = buffer.data();
    return ::memcmp(h, "GIF87a", 6) == 0
        || ::memcmp(h, "GIF89a", 6) == 0;
}

bool cFormatGif::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    m_filename = filename;

    cFile file;
    if (!openFile(file, filename, info))
    {
        cLog::Error("Can't open GIF image.");
        return false;
    }
    file.close();

    m_gif.reset(OpenFile(filename));

    if (m_gif.get() == nullptr)
    {
        cLog::Error("Can't open GIF image: '{}'.", GetError(m_gif.get()));
        return false;
    }

    int res = DGifSlurp(m_gif.get());
    if (res != GIF_OK)
    {
        cLog::Error("Can't read GIF image: '{}'.", GetError(m_gif.get()));
        return false;
    }

    if (m_gif->ImageCount < 1)
    {
        cLog::Error("Invalid GIF image count: {}.", m_gif->ImageCount);
        return false;
    }

    info.images = m_gif->ImageCount;
    info.isAnimation = info.images > 1;

    chunk.width = m_gif->SWidth;
    chunk.height = m_gif->SHeight;

    chunk.allocate(chunk.width, chunk.height, 32, ePixelFormat::RGBA);

    m_prevFrame = {};

    return load(0, chunk, info);
}

bool cFormatGif::LoadSubImageImpl(uint32_t current, sChunkData& chunk, sImageInfo& info)
{
    return load(current, chunk, info);
}

bool cFormatGif::load(uint32_t current, sChunkData& chunk, sImageInfo& info)
{
    info.current = std::max<uint32_t>(current, 0);
    info.current = std::min<uint32_t>(info.current, info.images - 1);

    const auto& image = m_gif->SavedImages[info.current];

    auto cmap = image.ImageDesc.ColorMap;
    if (cmap == nullptr)
    {
        cmap = m_gif->SColorMap;
    }
    if (cmap == nullptr)
    {
        cLog::Error("Invalid GIF colormap.");
        return false;
    }

    if (info.images == 1)
    {
        // use frame size instead 'canvas' size
        chunk.width = image.ImageDesc.Width;
        chunk.height = image.ImageDesc.Height;
    }

    info.bppImage = cmap->BitsPerPixel;

    info.delay = 100; // default value

    // Parse GCE for current frame
    uint32_t transparentIdx = static_cast<uint32_t>(-1);
    uint32_t disposalMode = 0;
    for (int i = 0; i < image.ExtensionBlockCount; i++)
    {
        const auto& eb = image.ExtensionBlocks[i];
        if (eb.ByteCount == 4 && eb.Function == 0xF9)
        {
            const bool hasTransparency = (eb.Bytes[0] & 1) == 1;
            if (hasTransparency)
            {
                transparentIdx = eb.Bytes[3];
            }

            disposalMode = (eb.Bytes[0] >> 2) & 0x07;

            // setup delay time in milliseconds
            uint32_t delay = (eb.Bytes[1] | (eb.Bytes[2] << 8)) * 10;
            if (delay != 0)
            {
                info.delay = delay;
            }
        }
    }

    // Apply previous frame's disposal before rendering current frame
    if (info.current == 0)
    {
        std::memset(chunk.bitmap.data(), 0, chunk.bitmap.size());
        m_prevFrame = {};
    }
    else if (m_prevFrame.disposalMode == 2)
    {
        // Clear previous frame's rectangle to transparent black
        for (uint32_t y = m_prevFrame.top; y < m_prevFrame.top + m_prevFrame.height && y < chunk.height; y++)
        {
            auto* row = &chunk.bitmap[y * chunk.pitch + m_prevFrame.left * 4];
            std::memset(row, 0, std::min(m_prevFrame.width, chunk.width - m_prevFrame.left) * 4);
        }
    }
    // Mode 0/1: leave canvas as-is (overlay)
    // Mode 3: not implemented (needs backup buffer — rare, skip for now)

    const uint32_t width = image.ImageDesc.Width;
    const uint32_t height = image.ImageDesc.Height;

    if (image.ImageDesc.Interlace)
    {
        // Need to perform 4 passes on the images:
        struct Interlace
        {
            uint32_t offset;
            uint32_t jump;
        };
        const Interlace interlaced[] = {
            { 0, 8 },
            { 4, 8 },
            { 2, 4 },
            { 1, 2 },
        };

        uint32_t row = 0;

        for (const auto& interlace : interlaced)
        {
            for (uint32_t y = interlace.offset; y < height; y += interlace.jump)
            {
                putRow(chunk, y, width, image, cmap, transparentIdx);

                updateProgress(static_cast<float>(row) / height);
                row++;
            }
        }

        info.formatName = "gif/i";
    }
    else
    {
        for (uint32_t y = 0; y < height; y++)
        {
            putRow(chunk, y, width, image, cmap, transparentIdx);

            updateProgress(static_cast<float>(y) / height);
        }

        info.formatName = "gif/p";
    }

    // Save current frame's disposal info for next frame
    m_prevFrame.disposalMode = disposalMode;
    m_prevFrame.left = image.ImageDesc.Left;
    m_prevFrame.top = image.ImageDesc.Top;
    m_prevFrame.width = width;
    m_prevFrame.height = height;

    return true;
}

#endif

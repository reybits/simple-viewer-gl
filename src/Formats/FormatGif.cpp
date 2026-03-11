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

    void putPixel(sChunkData& chunk, sImageInfo& info, uint32_t pos, const GifColorType* color, bool transparent)
    {
        if (!info.current || !transparent)
        {
            chunk.bitmap[pos + 0] = color->Red;
            chunk.bitmap[pos + 1] = color->Green;
            chunk.bitmap[pos + 2] = color->Blue;
            chunk.bitmap[pos + 3] = transparent ? 0 : 255;
        }
    }

    void putRow(sChunkData& chunk, sImageInfo& info, uint32_t row, uint32_t width, const SavedImage& image, const ColorMapObject* cmap, uint32_t transparentIdx)
    {
        for (uint32_t x = 0; x < width; x++)
        {
            const uint32_t idx = image.RasterBits[row * width + x];
            const uint32_t pos = (row + image.ImageDesc.Top) * chunk.pitch + (x + image.ImageDesc.Left) * 4;
            putPixel(chunk, info, pos, &cmap->Colors[idx], transparentIdx == idx);
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

    // look for the transparent color extension
    uint32_t transparentIdx = (uint32_t)-1U;
    for (int i = 0; i < image.ExtensionBlockCount; i++)
    {
        const auto& eb = image.ExtensionBlocks[i];
        if (eb.ByteCount == 4)
        {
            if (eb.Function == 0xF9)
            {
                const bool hasTransparency = (eb.Bytes[0] & 1) == 1;
                if (hasTransparency)
                {
                    transparentIdx = eb.Bytes[3];
                }

                const uint32_t disposalMode = (eb.Bytes[0] >> 2) & 0x07;
                cLog::Debug("Disposal: {} at frame {}.", disposalMode, info.current);
                // DISPOSAL_UNSPECIFIED 0 // No disposal specified.
                // DISPOSE_DO_NOT       1 // Leave image in place
                // DISPOSE_BACKGROUND   2 // Set area too background color
                // DISPOSE_PREVIOUS     3 // Restore to previous content
                if (disposalMode == 2)
                {
                    ::memset(chunk.bitmap.data(), 0, chunk.bitmap.size());
                }
            }

            // setup delay time in milliseconds
            uint32_t delay = (eb.Bytes[1] | (eb.Bytes[2] << 8)) * 10;
            if (delay != 0)
            {
                info.delay = delay;
            }
        }
    }

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
                putRow(chunk, info, y, width, image, cmap, transparentIdx);

                updateProgress((float)row / height);
                row++;
            }
        }

        info.formatName = "gif/i";
    }
    else
    {
        for (uint32_t y = 0; y < height; y++)
        {
            putRow(chunk, info, y, width, image, cmap, transparentIdx);

            updateProgress((float)y / height);
        }

        info.formatName = "gif/p";
    }

    return true;
}

#endif

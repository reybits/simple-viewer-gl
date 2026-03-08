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
#include "Common/BitmapDescription.h"
#include "Common/File.h"
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

    void putPixel(sBitmapDescription& desc, uint32_t pos, const GifColorType* color, bool transparent)
    {
        if (!desc.current || !transparent)
        {
            desc.bitmap[pos + 0] = color->Red;
            desc.bitmap[pos + 1] = color->Green;
            desc.bitmap[pos + 2] = color->Blue;
            desc.bitmap[pos + 3] = transparent ? 0 : 255;
        }
    }

    void putRow(sBitmapDescription& desc, uint32_t row, uint32_t width, const SavedImage& image, const ColorMapObject* cmap, uint32_t transparentIdx)
    {
        for (uint32_t x = 0; x < width; x++)
        {
            const uint32_t idx = image.RasterBits[row * width + x];
            const uint32_t pos = (row + image.ImageDesc.Top) * desc.pitch + (x + image.ImageDesc.Left) * 4;
            putPixel(desc, pos, &cmap->Colors[idx], transparentIdx == idx);
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

bool cFormatGif::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    m_filename = filename;

    cFile file;
    if (!openFile(file, filename, desc))
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

    desc.images = m_gif->ImageCount;
    desc.isAnimation = desc.images > 1;

    desc.width = m_gif->SWidth;
    desc.height = m_gif->SHeight;

    desc.allocate(desc.width, desc.height, 32, ePixelFormat::RGBA);

    return load(0, desc);
}

bool cFormatGif::LoadSubImageImpl(uint32_t current, sBitmapDescription& desc)
{
    return load(current, desc);
}

bool cFormatGif::load(uint32_t current, sBitmapDescription& desc)
{
    desc.current = std::max<uint32_t>(current, 0);
    desc.current = std::min<uint32_t>(desc.current, desc.images - 1);

    const auto& image = m_gif->SavedImages[desc.current];

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

    if (desc.images == 1)
    {
        // use frame size instead 'canvas' size
        desc.width = image.ImageDesc.Width;
        desc.height = image.ImageDesc.Height;
    }

    desc.bppImage = cmap->BitsPerPixel;

    desc.delay = 100; // default value

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
                // ::printf("Disposal: %u at frame %u\n", disposalMode, desc.current);
                // DISPOSAL_UNSPECIFIED 0 // No disposal specified.
                // DISPOSE_DO_NOT       1 // Leave image in place
                // DISPOSE_BACKGROUND   2 // Set area too background color
                // DISPOSE_PREVIOUS     3 // Restore to previous content
                if (disposalMode == 2)
                {
                    ::memset(desc.bitmap.data(), 0, desc.bitmap.size());
                }
            }

            // setup delay time in milliseconds
            uint32_t delay = (eb.Bytes[1] | (eb.Bytes[2] << 8)) * 10;
            if (delay != 0)
            {
                desc.delay = delay;
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
                putRow(desc, y, width, image, cmap, transparentIdx);

                updateProgress((float)row / height);
                row++;
            }
        }

        desc.formatName = "gif/i";
    }
    else
    {
        for (uint32_t y = 0; y < height; y++)
        {
            putRow(desc, y, width, image, cmap, transparentIdx);

            updateProgress((float)y / height);
        }

        desc.formatName = "gif/p";
    }

    return true;
}

#endif

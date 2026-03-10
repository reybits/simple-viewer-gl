/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatTarga.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/Helpers.h"
#include "Common/ImageInfo.h"
#include "Log/Log.h"

namespace
{
    enum class ImageType : uint8_t
    {
        Colormap = 1,
        RGB = 2,
        Monochrome = 3,
        RLEColormap = 9,
        RLERGB = 10,
        RLEMonochrome = 11,
    };

#pragma pack(push, 1)
    struct sTARGAHeader
    {
        uint8_t idLength;
        uint8_t colorMapType;
        ImageType imageType;

        uint16_t firstEntryIndex;
        uint16_t colorMapLength;
        uint8_t colorMapEntrySize;

        uint16_t xOrigin;
        uint16_t yOrigin;
        uint16_t width;
        uint16_t height;
        uint8_t pixelDepth;
        uint8_t imageDescriptor;
    };
#pragma pack(pop)

    enum class Origin
    {
#if 0
        LowerRight,
        UpperRight,
#endif
        LowerLeft,
        UpperLeft,
    };

    Origin getOrigin(uint8_t imageDescriptor)
    {
        // return (Origin)((imageDescriptor >> 4) & 0x03);
        return (imageDescriptor & (1 << 5)) ? Origin::UpperLeft : Origin::LowerLeft;
    }

    inline uint32_t getIndexUpperLeft(uint32_t x, uint32_t y, const sChunkData& chunk)
    {
        const uint32_t components = chunk.bpp / 8;
        return y * chunk.pitch + x * components;
    }

    inline uint32_t getIndexLowerLeft(uint32_t x, uint32_t y, const sChunkData& chunk)
    {
        const uint32_t components = chunk.bpp / 8;
        return (chunk.height - y - 1) * chunk.pitch + x * components;
    }

#if 0
    inline uint32_t getIndexUpperRight(uint32_t x, uint32_t y, const sChunkData& chunk)
    {
        const uint32_t components = chunk.bpp / 8;
        return y * chunk.pitch + (chunk.width - x - 1) * components;
    }

    inline uint32_t getIndexLowerRight(uint32_t x, uint32_t y, const sChunkData& chunk)
    {
        const uint32_t components = chunk.bpp / 8;
        return (chunk.height - y - 1) * chunk.pitch + (chunk.width - x - 1) * components;
    }
#endif

    inline uint32_t getIndex(uint32_t x, uint32_t y, const sChunkData& chunk, Origin origin)
    {
        const uint32_t idx = origin == Origin::LowerLeft
            ? getIndexLowerLeft(x, y, chunk)
            : getIndexUpperLeft(x, y, chunk);
        return idx;
    }

    bool colormapped(const sTARGAHeader& header, const uint8_t* tga, sChunkData& chunk, sImageInfo& info)
    {
        if (header.colorMapType != 1)
        {
            cLog::Error("Unknown color-mapped format.");
            return false;
        }

        if (header.pixelDepth != 8)
        {
            cLog::Error("Non 8 bit color-mapped format.");
            return false;
        }

        if (header.colorMapEntrySize != 24)
        {
            cLog::Error("8 bit with non 24 bit color map entry size currently not supported.");
        }

        info.bppImage = 8;
        chunk.bpp = 24;
        chunk.pitch = helpers::calculatePitch(chunk.width, chunk.bpp);
        chunk.resizeBitmap(chunk.pitch, chunk.height);
        auto out = chunk.bitmap.data();

        auto cmdData = tga + header.idLength;
        const uint32_t cmtWidth = header.colorMapEntrySize / 8;
        tga += header.colorMapLength * cmtWidth;

        const auto origin = getOrigin(header.imageDescriptor);

        if (header.imageType == ImageType::Colormap)
        {
            // ::printf("(II) Uncompressed color-mapped image.\n");

            uint32_t sp = 0;
            for (uint32_t y = 0; y < header.height; y++)
            {
                uint32_t dp = getIndex(0, y, chunk, origin);
                for (uint32_t x = 0; x < header.width; x++)
                {
                    out[dp + 0] = cmdData[tga[sp] * cmtWidth + 2];
                    out[dp + 1] = cmdData[tga[sp] * cmtWidth + 1];
                    out[dp + 2] = cmdData[tga[sp] * cmtWidth + 0];
                    dp += 3;
                    sp++;
                }
            }

            return true;
        }
        else if (header.imageType == ImageType::RLEColormap)
        {
            // ::printf("(II) Compressed color-mapped image.\n");

            uint32_t sp = 0;
            uint32_t x = 0;
            uint32_t y = 0;

            while (y < header.height)
            {
                const uint8_t cunkHead = tga[sp++];
                const bool isPacked = (cunkHead & 128) != 0;
                const uint8_t count = (cunkHead & 127) + 1;

                if (isPacked)
                {
                    const uint8_t b = cmdData[tga[sp] * cmtWidth + 0];
                    const uint8_t g = cmdData[tga[sp] * cmtWidth + 1];
                    const uint8_t r = cmdData[tga[sp] * cmtWidth + 2];
                    sp++;

                    for (uint32_t i = 0; i < count; i++)
                    {
                        if (x == header.width)
                        {
                            x = 0;
                            y++;
                            if (y == header.height)
                            {
                                break;
                            }
                        }
                        const uint32_t dp = getIndex(x, y, chunk, origin);
                        out[dp + 0] = r;
                        out[dp + 1] = g;
                        out[dp + 2] = b;
                        x++;
                    }
                }
                else
                {
                    for (uint32_t i = 0; i < count; i++)
                    {
                        if (x == header.width)
                        {
                            x = 0;
                            y++;
                            if (y == header.height)
                            {
                                break;
                            }
                        }
                        const uint32_t dp = getIndex(x, y, chunk, origin);
                        out[dp + 0] = cmdData[tga[sp] * cmtWidth + 2];
                        out[dp + 1] = cmdData[tga[sp] * cmtWidth + 1];
                        out[dp + 2] = cmdData[tga[sp] * cmtWidth + 0];
                        sp++;
                        x++;
                    }
                }
            }

            return true;
        }

        return false;
    }

    bool rgbUncompressed(const sTARGAHeader& header, const uint8_t* tga, sChunkData& chunk, sImageInfo& info)
    {
        if (header.colorMapType != 0)
        {
            cLog::Error("Unknown color-mapped format.");
            return false;
        }

        const auto origin = getOrigin(header.imageDescriptor);
        uint32_t sp = 0;

        if (header.pixelDepth == 16)
        {
            info.bppImage = 16;
            chunk.bpp = 24;
            chunk.pitch = helpers::calculatePitch(chunk.width, chunk.bpp);
            chunk.resizeBitmap(chunk.pitch, chunk.height);
            auto out = chunk.bitmap.data();

            for (uint32_t y = 0; y < header.height; y++)
            {
                uint32_t dp = getIndex(0, y, chunk, origin);
                for (uint32_t x = 0; x < header.width; x++)
                {
                    auto c = *(uint16_t*)&tga[sp];
                    out[dp + 0] = (((c >> 10) & 31) * 255) / 31;
                    out[dp + 1] = (((c >> 5) & 31) * 255) / 31;
                    out[dp + 2] = (((c >> 0) & 31) * 255) / 31;
                    dp += 3;
                    sp += 2;
                }
            }
        }
        else if (header.pixelDepth == 24)
        {
            info.bppImage = 24;
            chunk.bpp = 24;
            chunk.pitch = helpers::calculatePitch(chunk.width, chunk.bpp);
            chunk.resizeBitmap(chunk.pitch, chunk.height);
            auto out = chunk.bitmap.data();

            for (uint32_t y = 0; y < header.height; y++)
            {
                uint32_t dp = getIndex(0, y, chunk, origin);
                for (uint32_t x = 0; x < header.width; x++)
                {
                    out[dp + 0] = tga[sp + 2];
                    out[dp + 1] = tga[sp + 1];
                    out[dp + 2] = tga[sp + 0];
                    dp += 3;
                    sp += 3;
                }
            }
        }
        else if (header.pixelDepth == 32)
        {
            chunk.bpp = 32;
            info.bppImage = 32;
            chunk.pitch = helpers::calculatePitch(chunk.width, chunk.bpp);
            chunk.resizeBitmap(chunk.pitch, chunk.height);
            auto out = chunk.bitmap.data();

            for (uint32_t y = 0; y < header.height; y++)
            {
                uint32_t dp = getIndex(0, y, chunk, origin);
                for (uint32_t x = 0; x < header.width; x++)
                {
                    out[dp + 0] = tga[sp + 2];
                    out[dp + 1] = tga[sp + 1];
                    out[dp + 2] = tga[sp + 0];
                    out[dp + 3] = tga[sp + 3];
                    dp += 4;
                    sp += 4;
                }
            }
        }
        else
        {
            cLog::Error("Unsupported uncompressed RGB format.");
            return false;
        }

        return true;
    }

    bool rgbCompressed(const sTARGAHeader& header, const uint8_t* tga, sChunkData& chunk, sImageInfo& info)
    {
        if (header.colorMapType != 0)
        {
            cLog::Error("Unknown color-mapped format.");
            return false;
        }

        const auto origin = getOrigin(header.imageDescriptor);
        uint32_t x = 0;
        uint32_t y = 0;
        uint32_t sp = 0;

        if (header.pixelDepth == 16)
        {
            info.bppImage = 16;
            chunk.bpp = 24;
            chunk.pitch = helpers::calculatePitch(chunk.width, chunk.bpp);
            chunk.resizeBitmap(chunk.pitch, chunk.height);
            auto out = chunk.bitmap.data();

            while (y < header.height)
            {
                const uint8_t cunkHead = tga[sp++];
                const bool isPacked = (cunkHead & 128) != 0;
                const uint8_t count = (cunkHead & 127) + 1;

                if (isPacked == false)
                {
                    for (uint32_t i = 0; i < count; i++)
                    {
                        if (x == header.width)
                        {
                            x = 0;
                            y++;
                            if (y == header.height)
                            {
                                break;
                            }
                        }
                        auto c = *(uint16_t*)&tga[sp];
                        const uint32_t dp = getIndex(x, y, chunk, origin);
                        out[dp + 0] = (uint8_t)(((c >> 0) & 31) * 255) / 31;
                        out[dp + 1] = (uint8_t)(((c >> 5) & 31) * 255) / 31;
                        out[dp + 2] = (uint8_t)(((c >> 10) & 31) * 255) / 31;
                        sp += 2;
                        x++;
                    }
                }
                else
                {
                    const auto c = *(uint16_t*)&tga[sp];
                    const auto r = (uint8_t)(((c >> 0) & 31) * 255) / 31;
                    const auto g = (uint8_t)(((c >> 5) & 31) * 255) / 31;
                    const auto b = (uint8_t)(((c >> 10) & 31) * 255) / 31;
                    sp += 2;

                    for (uint32_t i = 0; i < count; i++)
                    {
                        if (x == header.width)
                        {
                            x = 0;
                            y++;
                            if (y == header.height)
                            {
                                break;
                            }
                        }
                        const uint32_t dp = getIndex(x, y, chunk, origin);
                        out[dp + 0] = r;
                        out[dp + 1] = g;
                        out[dp + 2] = b;
                        x++;
                    }
                }
            }
        }
        else if (header.pixelDepth == 24)
        {
            info.bppImage = 24;
            chunk.bpp = 24;
            chunk.pitch = helpers::calculatePitch(chunk.width, chunk.bpp);
            chunk.resizeBitmap(chunk.pitch, chunk.height);
            auto out = chunk.bitmap.data();

            while (y < header.height)
            {
                const uint8_t cunkHead = tga[sp++];
                const bool isPacked = (cunkHead & 128) != 0;
                const uint8_t count = (cunkHead & 127) + 1;

                if (isPacked == false)
                {
                    for (uint32_t i = 0; i < count; i++)
                    {
                        if (x == header.width)
                        {
                            x = 0;
                            y++;
                            if (y == header.height)
                            {
                                break;
                            }
                        }
                        const uint32_t dp = getIndex(x, y, chunk, origin);
                        out[dp + 0] = tga[sp + 2];
                        out[dp + 1] = tga[sp + 1];
                        out[dp + 2] = tga[sp + 0];
                        sp += 3;
                        x++;
                    }
                }
                else
                {
                    const uint8_t b = tga[sp + 0];
                    const uint8_t g = tga[sp + 1];
                    const uint8_t r = tga[sp + 2];
                    sp += 3;
                    for (uint32_t i = 0; i < count; i++)
                    {
                        if (x == header.width)
                        {
                            x = 0;
                            y++;
                            if (y == header.height)
                            {
                                break;
                            }
                        }
                        const uint32_t dp = getIndex(x, y, chunk, origin);
                        out[dp + 0] = r;
                        out[dp + 1] = g;
                        out[dp + 2] = b;
                        x++;
                    }
                }
            }
        }
        else if (header.pixelDepth == 32)
        {
            info.bppImage = 32;
            chunk.bpp = 32;
            chunk.pitch = helpers::calculatePitch(chunk.width, chunk.bpp);
            chunk.resizeBitmap(chunk.pitch, chunk.height);
            auto out = chunk.bitmap.data();

            while (y < header.height)
            {
                const uint8_t cunkHead = tga[sp++];
                const bool isPacked = (cunkHead & 128) != 0;
                const uint8_t count = (cunkHead & 127) + 1;

                if (isPacked == false)
                {
                    for (uint32_t i = 0; i < count; i++)
                    {
                        if (x == header.width)
                        {
                            x = 0;
                            y++;
                            if (y == header.height)
                            {
                                break;
                            }
                        }
                        const uint32_t dp = getIndex(x, y, chunk, origin);
                        out[dp + 0] = tga[sp + 2];
                        out[dp + 1] = tga[sp + 1];
                        out[dp + 2] = tga[sp + 0];
                        out[dp + 3] = tga[sp + 3];
                        sp += 4;
                        x++;
                    }
                }
                else
                {
                    const uint8_t b = tga[sp + 0];
                    const uint8_t g = tga[sp + 1];
                    const uint8_t r = tga[sp + 2];
                    const uint8_t a = tga[sp + 3];
                    sp += 4;

                    for (uint32_t i = 0; i < count; i++)
                    {
                        if (x == header.width)
                        {
                            x = 0;
                            y++;
                            if (y == header.height)
                            {
                                break;
                            }
                        }
                        const uint32_t dp = getIndex(x, y, chunk, origin);
                        out[dp + 0] = r;
                        out[dp + 1] = g;
                        out[dp + 2] = b;
                        out[dp + 3] = a;
                        x++;
                    }
                }
            }
        }
        else
        {
            cLog::Error("Unsupported compressed RGB format.");
            return false;
        }

        return true;
    }
} // namespace

bool cFormatTarga::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, sizeof(sTARGAHeader)))
    {
        return false;
    }

    const auto h = reinterpret_cast<const sTARGAHeader*>(buffer.data());
    return h->colorMapType <= 1
        && h->width > 0 && h->height > 0
        && (h->imageType == ImageType::Colormap
            || h->imageType == ImageType::RGB
            || h->imageType == ImageType::Monochrome
            || h->imageType == ImageType::RLEColormap
            || h->imageType == ImageType::RLERGB
            || h->imageType == ImageType::RLEMonochrome)
        && (h->pixelDepth == 8 || h->pixelDepth == 16 || h->pixelDepth == 24 || h->pixelDepth == 32);
}

bool cFormatTarga::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (!openFile(file, filename, info))
    {
        return false;
    }

    std::vector<uint8_t> tga(info.fileSize);
    if (info.fileSize != file.read(tga.data(), info.fileSize))
    {
        cLog::Error("Can't read TARGA data.");
        return false;
    }

    auto& header = *reinterpret_cast<const sTARGAHeader*>(tga.data());
    auto tga_data = reinterpret_cast<const uint8_t*>(tga.data() + sizeof(sTARGAHeader));

    chunk.width = header.width;
    chunk.height = header.height;

#if defined(_DEBUG)
    cLog::Debug("idLength:          {}", (uint32_t)header.idLength);
    cLog::Debug("colorMapType:      {}", (uint32_t)header.colorMapType);
    cLog::Debug("imageType:         {}", (uint32_t)header.imageType);
    cLog::Debug("firstEntryIndex:   {}", (uint32_t)header.firstEntryIndex);
    cLog::Debug("colorMapLength:    {}", (uint32_t)header.colorMapLength);
    cLog::Debug("colorMapEntrySize: {}", (uint32_t)header.colorMapEntrySize);
    cLog::Debug("xOrigin:           {}", (uint32_t)header.xOrigin);
    cLog::Debug("yOrigin:           {}", (uint32_t)header.yOrigin);
    cLog::Debug("width:             {}", (uint32_t)header.width);
    cLog::Debug("height:            {}", (uint32_t)header.height);
    cLog::Debug("pixelDepth:        {}", (uint32_t)header.pixelDepth);
    cLog::Debug("imageDescriptor:   {}", (uint32_t)header.imageDescriptor);
#endif

    bool result = false;

    // 1 - Uncompressed, color-mapped images.
    // 2 - Uncompressed, RGB images.
    // 3 - Uncompressed, black and white images.
    // 9 - Runlength encoded color-mapped images.
    // 10 - Runlength encoded RGB images.
    // 11 - Compressed, black and white images.
    // 32 - Compressed color-mapped data, using Huffman, Delta, and runlength encoding.
    // 33 - Compressed color-mapped data, using Huffman, Delta, and runlength encoding. 4-pass quadtree-type process.
    if (header.imageType == ImageType::Colormap || header.imageType == ImageType::RLEColormap)
    {
        result = colormapped(header, tga_data, chunk, info);
    }
    else if (header.imageType == ImageType::RGB)
    {
        result = rgbUncompressed(header, tga_data, chunk, info);
    }
    else if (header.imageType == ImageType::RLERGB)
    {
        result = rgbCompressed(header, tga_data, chunk, info);
    }

    if (result == false)
    {
        cLog::Error("Unknown image type.");
        return false;
    }

    info.formatName = (header.imageType == ImageType::RLEColormap
                            || header.imageType == ImageType::RLERGB
                            || header.imageType == ImageType::RLEMonochrome)
        ? "targa/rle"
        : "targa";

    chunk.format = chunk.bpp == 32 ? ePixelFormat::RGBA : ePixelFormat::RGB;

    return true;
}

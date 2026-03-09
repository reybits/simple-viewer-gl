/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#if defined(OPENEXR_SUPPORT)

#include "FormatExr.h"
#include "Common/BitmapDescription.h"
#include "Common/File.h"
#include "Common/Helpers.h"
#include "Log/Log.h"

#include <OpenEXR/ImfArray.h>
#include <OpenEXR/ImfPreviewImage.h>
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfStandardAttributes.h>
#include <OpenEXR/ImfTiledRgbaFile.h>
#include <algorithm>
#include <cstring>

namespace
{
    struct sRgba8888
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        uint8_t a;
    };

    struct sRgb888
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    inline uint8_t HalfToUint8(const half& h)
    {
        return static_cast<uint8_t>(std::clamp(static_cast<uint32_t>(h * 255.0f), 0u, 255u));
    }

    const char* GetFormat(uint32_t format)
    {
        const char* Formats[] = {
            "exr",       // no compression
            "exr/rle",   // run length encoding
            "exr/zips",  // zlib compression, one scan line at a time
            "exr/zip",   // zlib compression, in blocks of 16 scan lines
            "exr/piz",   // piz-based wavelet compression
            "exr/pxr24", // lossy 24-bit float compression
            "exr/b44",   // lossy 4-by-4 pixel block compression, fixed compression rate
            "exr/b44a",  // lossy 4-by-4 pixel block compression, flat fields are compressed more
            "exr/dwaa",  // lossy DCT based compression, in blocks of 32 scanlines. More efficient for partial buffer access.

            "exr/dwab", // lossy DCT based compression, in blocks
            // of 256 scanlines. More efficient space
            // wise and faster to decode full frames
            // than DWAA_COMPRESSION.
        };

        return format < std::size(Formats) ? Formats[format] : "unknown";
    }

    using eCategory = sBitmapDescription::ExifCategory;

    void ReadStringField(const Imf::Header& header, const char* field, const char* title, eCategory category, sBitmapDescription::ExifList& exifList)
    {
        auto value = header.findTypedAttribute<Imf::StringAttribute>(field);
        if (value != nullptr)
        {
            exifList.push_back({ category, title, value->value() });
        }
    }

    void ReadHeader(const Imf::Header& header, sBitmapDescription& desc)
    {
        auto& exifList = desc.exifList;

        ReadStringField(header, "owner", "Owner", eCategory::Info, exifList);
        ReadStringField(header, "capDate", "Date", eCategory::Date, exifList);
        ReadStringField(header, "comments", "Comments", eCategory::Info, exifList);
        ReadStringField(header, "type", "Type", eCategory::Info, exifList);

#if 0
        for (auto it = header.begin(), itEnd = header.end(); it != itEnd; ++it)
        {
            auto& attr = it.attribute();
            exifList.push_back({ eCategory::Other, it.name(), attr.typeName() });
        }
#endif
    }

    bool ReadTiledRgba(const char* filename, Imf::Array2D<Imf::Rgba>& pixels, sBitmapDescription& desc, Imf::RgbaChannels& channels, uint32_t& compression)
    {
        Imf::TiledRgbaInputFile in(filename);
        bool result = in.isComplete();
        if (result)
        {
            channels = in.channels();
            compression = in.compression();
            auto& header = in.header();
            ReadHeader(header, desc);

            auto& dw = in.dataWindow();
            const auto width = dw.max.x - dw.min.x + 1;
            const auto height = dw.max.y - dw.min.y + 1;
            desc.width = width;
            desc.height = height;

            const auto dx = dw.min.x;
            const auto dy = dw.min.y;

            pixels.resizeErase(height, width);
            in.setFrameBuffer(&pixels[-dy][-dx], 1, width);
            in.readTiles(0, in.numXTiles() - 1, 0, in.numYTiles() - 1);
        }

        return result;
    }

    bool ReadScanlineRgba(const char* filename, Imf::Array2D<Imf::Rgba>& pixels, sBitmapDescription& desc, Imf::RgbaChannels& channels, uint32_t& compression)
    {
        Imf::RgbaInputFile in(filename);
        bool result = in.isComplete();

        if (result)
        {
            channels = in.channels();
            compression = in.compression();
            auto& header = in.header();
            ReadHeader(header, desc);

            auto& dw = in.dataWindow();
            const auto width = dw.max.x - dw.min.x + 1;
            const auto height = dw.max.y - dw.min.y + 1;
            desc.width = width;
            desc.height = height;

            const auto dx = dw.min.x;
            const auto dy = dw.min.y;

            pixels.resizeErase(height, width);
            in.setFrameBuffer(&pixels[-dx][-dy], 1, width);
            in.readPixels(dw.min.y, dw.max.y);
        }

        return result;
    }

#if 0
    bool loadPreview(Imf::RgbaInputFile& in, sBitmapDescription& desc)
    {
        auto& header = in.header();
        const bool hasPreview = header.hasPreviewImage();
        if (hasPreview)
        {
            const auto& preview = header.previewImage();

            const uint32_t width = preview.width();
            const uint32_t height = preview.height();

            desc.width = width;
            desc.height = height;
            desc.bppImage = 0;

            desc.pitch = desc.width * 4;
            desc.bitmap.resize(desc.pitch * desc.height);

            desc.bpp = 32;
            desc.format = ePixelFormat::RGBA;

            auto bitmap = reinterpret_cast<sRgba8888*>(desc.bitmap.data());

            for (uint32_t y = 0; y < height; ++y)
            {
                size_t idx = y * width;
                for (uint32_t x = 0; x < width; ++x)
                {
                    const auto& p = preview.pixel(x, y);

                    bitmap[idx].r = HalfToUint8(p.r);
                    bitmap[idx].g = HalfToUint8(p.g);
                    bitmap[idx].b = HalfToUint8(p.b);
                    bitmap[idx].a = HalfToUint8(p.a);
                    idx++;
                }
            }
            return true;
        }

        return false;
    }
#endif
} // namespace

bool cFormatExr::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, 4))
    {
        return false;
    }

    const auto h = buffer.data();
    return h[0] == 0x76 && h[1] == 0x2f && h[2] == 0x31 && h[3] == 0x01;
}

bool cFormatExr::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    cFile file;
    if (!openFile(file, filename, desc))
    {
        return false;
    }

    file.close();

    bool result = false;

    Imf::RgbaChannels channels = static_cast<Imf::RgbaChannels>(0u);
    Imf::Array2D<Imf::Rgba> pixels;
    uint32_t compression = ~0u;

    try
    {
        result = ReadScanlineRgba(filename, pixels, desc, channels, compression);
    }
    catch (...)
    {
        cLog::Error("Can't read scanline EXR data.");

        try
        {
            result = ReadTiledRgba(filename, pixels, desc, channels, compression);
        }
        catch (...)
        {
            result = false;
            cLog::Error("Can't read tiled EXR data.");
        }
    }

    if (result)
    {
        desc.images = 1;
        desc.current = 0;

        // WRITE_R    = 0x01, // Red
        // WRITE_G    = 0x02, // Green
        // WRITE_B    = 0x04, // Blue
        // WRITE_A    = 0x08, // Alpha
        // WRITE_Y    = 0x10, // Luminance, for black-and-white images, or in combination with chroma
        // WRITE_C    = 0x20, // Chroma (two subsampled channels, RY and BY, supported only for scanline-based files)
        //
        // WRITE_RGB  = 0x07, // Red, green, blue
        // WRITE_RGBA = 0x0f, // Red, green, blue, alpha
        // WRITE_YC   = 0x30, // Luminance, chroma
        // WRITE_YA   = 0x18, // Luminance, alpha
        // WRITE_YCA  = 0x38  // Luminance, chroma, alpha

        uint32_t chCount = 0;

        chCount += (channels & Imf::WRITE_R) != 0;
        chCount += (channels & Imf::WRITE_G) != 0;
        chCount += (channels & Imf::WRITE_B) != 0;
        chCount += (channels & Imf::WRITE_A) != 0;

        chCount += (channels & Imf::WRITE_Y) != 0;
        chCount += (channels & Imf::WRITE_C) != 0;

        desc.bppImage = chCount * 8;

        const bool hasA = (channels & Imf::WRITE_A) != 0;
        const uint32_t bytes = hasA ? 4 : 3;
        desc.bpp = bytes * 8;
        desc.format = hasA ? ePixelFormat::RGBA : ePixelFormat::RGB;
        desc.allocate(desc.width, desc.height, desc.bpp, desc.format);

        if (hasA)
        {
            auto src = &pixels[0][0];
            for (uint32_t y = 0; y < desc.height; y++)
            {
                auto dst = reinterpret_cast<sRgba8888*>(desc.bitmap.data() + y * desc.pitch);
                for (uint32_t x = 0; x < desc.width; x++)
                {
                    const auto& i = *src++;
                    dst[x].r = HalfToUint8(i.r);
                    dst[x].g = HalfToUint8(i.g);
                    dst[x].b = HalfToUint8(i.b);
                    dst[x].a = HalfToUint8(i.a);
                }
            }
        }
        else
        {
            auto src = &pixels[0][0];
            for (uint32_t y = 0; y < desc.height; y++)
            {
                auto dst = reinterpret_cast<sRgb888*>(desc.bitmap.data() + y * desc.pitch);
                for (uint32_t x = 0; x < desc.width; x++)
                {
                    const auto& i = *src++;
                    dst[x].r = HalfToUint8(i.r);
                    dst[x].g = HalfToUint8(i.g);
                    dst[x].b = HalfToUint8(i.b);
                }
            }
        }

        desc.formatName = GetFormat(compression);
    }

    return result;
}

#endif

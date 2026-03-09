/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatScr.h"
#include "Common/BitmapDescription.h"
#include "Common/File.h"
#include "Log/Log.h"

#include <cstdio>
#include <cstring>
#include <iterator>

namespace
{
    struct sColor
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    // palette PULSAR (0xcd)
    static constexpr sColor Palette[] = {
        // normal
        { 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0xcd },
        { 0xcd, 0x00, 0x00 },
        { 0xcd, 0x00, 0xcd },
        { 0x00, 0xcd, 0x00 },
        { 0x00, 0xcd, 0xcd },
        { 0xcd, 0xcd, 0x00 },
        { 0xcd, 0xcd, 0xcd },

        // bright
        { 0x00, 0x00, 0x00 },
        { 0x00, 0x00, 0xff },
        { 0xff, 0x00, 0x00 },
        { 0xff, 0x00, 0xff },
        { 0x00, 0xff, 0x00 },
        { 0x00, 0xff, 0xff },
        { 0xff, 0xff, 0x00 },
        { 0xff, 0xff, 0xff },
    };

    struct sPixelRGB
    {
        void set(bool isSet, uint8_t attribute)
        {
            const uint32_t bright = (attribute & 0x40) >> 3;
            if (isSet == false)
            {
                attribute >>= 3;
            }
            color = Palette[bright + (attribute & 0x07)];
        }

        void operator=(const sColor& other)
        {
            color = other;
        }

        sColor color;
    };

    struct ZXProperty
    {
        uint32_t cw; // canvas size
        uint32_t ch;

        uint32_t dw; // input bitmap size
        uint32_t dh;

        uint32_t dx;
        uint32_t dy;

        enum class Type : uint32_t
        {
            Scr,  // done
            ScS,  // done
            Bsc,  // done
            Atr,  // done
            Mc1,  // done
            Mc2,  // done
            Mc4,  // done
            BMc4, // done
            Img,
            Mgh,
            Mgs,
            // Chr,

            Unknown
        };

        Type type;

        const char* formatName;
    };

    ZXProperty GetType(uint32_t fileSize, const uint8_t* buffer)
    {
        struct ZXPropertyInternal
        {
            uint32_t size;
            ZXProperty prop;
        };
        static const ZXPropertyInternal sizesList[] = {
            { 6912, { 256, 192, 256, 192, 0, 0, ZXProperty::Type::Scr, "zx-scr" } },
            { 6929, { 256, 192, 256, 192, 0, 0, ZXProperty::Type::ScS, "zx-scr$" } },
            { 11136, { 384, 304, 256, 192, 64, 64, ZXProperty::Type::Bsc, "zx-bsc" } },
            { 768, { 256, 192, 256, 192, 0, 0, ZXProperty::Type::Atr, "zx-atr" } },
            { 12288, { 256, 192, 256, 192, 0, 0, ZXProperty::Type::Mc1, "zx-mc1" } },
            { 9216, { 256, 192, 256, 192, 0, 0, ZXProperty::Type::Mc2, "zx-mc2" } },
            { 7680, { 256, 192, 256, 192, 0, 0, ZXProperty::Type::Mc4, "zx-mc4" } },
            { 11904, { 384, 304, 256, 192, 64, 64, ZXProperty::Type::BMc4, "zx-bmc4" } },
            { 13824, { 256, 192, 256, 192, 0, 0, ZXProperty::Type::Img, "zx-img" } },
            // { 41479, { 256, 192, 256, 192, 0, 0, ZXProperty::Type::Chr, "zx-chr$" } },
        };

        for (size_t i = 0; i < std::size(sizesList); i++)
        {
            auto& s = sizesList[i];
            if (s.size == fileSize)
            {
                return s.prop;
            }
        }

        if (buffer[0] == 'M' && buffer[1] == 'G')
        {
            static char formatName[20];
            const uint32_t blockHeight = buffer[4];
            if (buffer[2] == 'H' && (fileSize == 19456 || fileSize == 18688 || fileSize == 15616 || fileSize == 14080))
            {
                // 19456 - mg1
                // 18688 - mg2
                // 15616 - mg4
                // 14080 - mg8
                ::snprintf(formatName, sizeof(formatName), "zx-mgh%u", blockHeight);
                return { 320, 240, 256, 192, 32, 24, ZXProperty::Type::Mgh, formatName };
            }
            else if (buffer[2] == 'S' && fileSize == 36871)
            {
                // 36871 - mgs
                ::snprintf(formatName, sizeof(formatName), "zx-mgs%u", blockHeight);
                return { 320, 240, 256, 192, 32, 24, ZXProperty::Type::Mgs, formatName };
            }
        }

        return { 0, 0, 0, 0, 0, 0, ZXProperty::Type::Unknown, "" };
    }

    void PutSixteenPixels(sPixelRGB* out, uint8_t color)
    {
        const uint8_t left = color & 0x07;
        for (uint32_t i = 0; i < 8; i++)
        {
            out->set(true, left);
            out++;
        }

        const uint8_t right = (color >> 3) & 0x07;
        for (uint32_t i = 0; i < 8; i++)
        {
            out->set(true, right);
            out++;
        }
    }

    sColor GetColorIntensity(uint8_t attr)
    {
        // uint8_t i = attr & 8 ? 1 : 0;

        uint8_t b = attr & 1 ? 1 : 0;
        uint8_t r = attr & 2 ? 1 : 0;
        uint8_t g = attr & 4 ? 1 : 0;

        return { r, g, b };
    }

    sColor MergeColors(uint8_t attr0, uint8_t attr1)
    {
        const auto c0 = GetColorIntensity(attr0);
        const auto c1 = GetColorIntensity(attr1);

        const bool b0 = (attr0 & 0x40) != 0;
        const bool b1 = (attr1 & 0x40) != 0;

        const bool z0 = (attr0 & 0x07) == 0;
        const bool z1 = (attr1 & 0x07) == 0;

        const bool n0 = !z0 && !b0;
        const bool n1 = !z1 && !b1;

        uint8_t idx = 0;
        if (z0 && z1)
        {
            idx = 0;
        }
        else if (n0 && n1)
        {
            idx = 1;
        }
        else if (b0 && b1)
        {
            idx = 2;
        }
        else if (z0 && n1)
        {
            idx = 3;
        }
        else if (n0 && b1)
        {
            idx = 4;
        }
        else if (z0 && b1)
        {
            idx = 5;
        }

        // pulsar
        constexpr uint8_t intensity[] = { 0x00, 0x76, 0xcd, 0xe9, 0xff, 0x9f };
        // 0x00 - ZZ - zero + zero
        // 0x76 - NN - normal + normal
        // 0xcd - BB - bright + bright
        // 0xe9 - ZN - zero + normal
        // 0xff - NB - normal + bright
        // 0x9f - ZB - zero + bright

        const uint32_t i = intensity[idx];

        const float v0 = 0.5f;
        const float v1 = 1.0f - v0;
        return {
            (uint8_t)(v0 * i * c0.r + v1 * i * c1.r),
            (uint8_t)(v0 * i * c0.g + v1 * i * c1.g),
            (uint8_t)(v0 * i * c0.b + v1 * i * c1.b),
        };
    }

    sColor MergeColors(const sColor& c0, const sColor& c1)
    {
        const float a = 0.5f;
        const float b = 1.0f - a;
        return sColor{
            (uint8_t)(c0.r * a + c1.r * b),
            (uint8_t)(c0.g * a + c1.g * b),
            (uint8_t)(c0.b * a + c1.b * b),
        };
    }

    void MakeBorder(sBitmapDescription& desc, const sColor& color)
    {
        auto pixel = (sPixelRGB*)desc.bitmap.data();

        for (uint32_t i = 0, size = desc.width * desc.height; i < size; i++)
        {
            pixel[i] = color;
        }
    }

    void MakeBorder(sBitmapDescription& desc, const uint8_t* zxBorder)
    {
        // top
        for (uint32_t y = 0; y < 64; y++)
        {
            auto out = (sPixelRGB*)(desc.bitmap.data() + y * desc.pitch);
            for (uint32_t x = 0; x < 24; x++)
            {
                const uint8_t color = *zxBorder++;
                PutSixteenPixels(out, color);
                out += 16;
            }
        }

        // left / right
        for (uint32_t y = 0; y < 192; y++)
        {
            auto out = (sPixelRGB*)(desc.bitmap.data() + (y + 64) * desc.pitch);
            for (uint32_t x = 0; x < 4; x++)
            {
                const uint8_t color = *zxBorder++;
                PutSixteenPixels(out, color);
                out += 16;
            }

            out += 256;
            for (uint32_t x = 0; x < 4; x++)
            {
                const uint8_t color = *zxBorder++;
                PutSixteenPixels(out, color);
                out += 16;
            }
        }

        // bottom
        for (uint32_t y = 0; y < 48; y++)
        {
            auto out = (sPixelRGB*)(desc.bitmap.data() + (y + 64 + 192) * desc.pitch);
            for (uint32_t x = 0; x < 24; x++)
            {
                const uint8_t color = *zxBorder++;
                PutSixteenPixels(out, color);
                out += 16;
            }
        }
    }

    void PutEightPixels(sPixelRGB* out, const uint8_t pixels, const uint8_t attribute)
    {
        for (uint32_t i = 0; i < 8; i++)
        {
            const uint32_t bit = 0x80 >> i;
            const bool isSet = pixels & bit;
            out->set(isSet, attribute);
            out++;
        }
    }

    void PutEightPixels(sPixelRGB* out, const uint8_t pixels[2], const uint8_t attributes[2])
    {
        for (uint32_t i = 0; i < 8; i++)
        {
            const uint32_t bit = 0x80 >> i;

            sPixelRGB pixel0;
            pixel0.set(pixels[0] & bit, attributes[0]);

            sPixelRGB pixel1;
            pixel1.set(pixels[1] & bit, attributes[1]);

            *out = MergeColors(pixel0.color, pixel1.color);
            out++;
        }
    }

    void FillThird(uint32_t layer, const uint8_t* zxPixels, const uint8_t* zxColors,
                   sBitmapDescription& desc, uint32_t blockHeight, sPixelRGB* out)
    {
        zxPixels += 2048 * layer;
        zxColors += 2048 / blockHeight * layer;

        for (uint32_t y = 0; y < 64; y++)
        {
            const uint32_t line = (y * 8) % 64 + (y * 8) / 64;
            auto startLine = &out[line * desc.width];
            for (uint32_t x = 0; x < 256 / 8; x++)
            {
                const uint8_t pixels = *zxPixels++;
                const uint8_t attribute = zxColors[(line / blockHeight) * 32 + x];
                PutEightPixels(&startLine[x * 8], pixels, attribute);
            }
        }
    }

    void FillThird(uint32_t layer, const uint8_t* zxPixels0, const uint8_t* zxPixels1,
                   const uint8_t* zxColors0, const uint8_t* zxColors1,
                   sBitmapDescription& desc, uint32_t blockHeight, sPixelRGB* out)
    {
        zxPixels0 += 2048 * layer;
        zxPixels1 += 2048 * layer;

        zxColors0 += 2048 / blockHeight * layer;
        zxColors1 += 2048 / blockHeight * layer;

        for (uint32_t y = 0; y < 64; y++)
        {
            const uint32_t line = (y * 8) % 64 + (y * 8) / 64;
            auto startLine = &out[line * desc.width];
            for (uint32_t x = 0; x < 256 / 8; x++)
            {
                const uint8_t pixels[2] = { *zxPixels0++, *zxPixels1++ };

                const uint32_t idx = (line / blockHeight) * 32 + x;
                const uint8_t attributes[2] = { zxColors0[idx], zxColors1[idx] };
                PutEightPixels(&startLine[x * 8], pixels, attributes);
            }
        }
    }

    void FillLinear(const uint8_t* zxPixels, const uint8_t* zxColors, uint32_t blockHeight, uint32_t outWidth, sPixelRGB* out)
    {
        for (uint32_t y = 0; y < 192; y++)
        {
            auto startLine = &out[y * outWidth];
            for (uint32_t x = 0; x < 256 / 8; x++)
            {
                const uint8_t pixels = *zxPixels++;
                const uint8_t attribute = zxColors[(y / blockHeight) * 32 + x];
                PutEightPixels(&startLine[x * 8], pixels, attribute);
            }
        }
    }

    void LoadScr(const uint8_t* buffer, sBitmapDescription& desc, const ZXProperty& prop)
    {
        if (prop.type == ZXProperty::Type::ScS)
        {
            desc.exifList.push_back({ sBitmapDescription::ExifCategory::Info, "Comment", (const char*)buffer });
            buffer += 17;
        }
        const uint8_t* zxPixels = buffer;
        const uint8_t* zxColors = buffer + 6144;

        for (uint32_t i = 0; i < 3; i++)
        {
            auto out = (sPixelRGB*)(desc.bitmap.data() + ((prop.dy + 64 * i) * desc.pitch)) + prop.dx;
            FillThird(i, zxPixels, zxColors, desc, 8, out);
        }
    }

    void LoadBsc(const uint8_t* buffer, sBitmapDescription& desc, const ZXProperty& prop)
    {
        const uint8_t* zxPixels = buffer;
        const uint8_t* zxColors = buffer + 6144;

        for (uint32_t i = 0; i < 3; i++)
        {
            auto out = (sPixelRGB*)(desc.bitmap.data() + ((prop.dy + 64 * i) * desc.pitch)) + prop.dx;
            FillThird(i, zxPixels, zxColors, desc, 8, out);
        }

        const uint8_t* zxBorder = buffer + 6144 + 768;
        MakeBorder(desc, zxBorder);
    }

    void LoadAtr(const uint8_t* buffer, sBitmapDescription& desc, const ZXProperty& prop)
    {
        const uint8_t px[2] = { 0x55, 0xaa }; // { 0b01010101, 0b10101010 };

        const uint8_t* zxColors = buffer;
        auto out = (sPixelRGB*)(desc.bitmap.data() + prop.dy * desc.pitch) + prop.dx;

        for (uint32_t y = 0; y < 192; y++)
        {
            auto startLine = &out[y * 256];
            for (uint32_t x = 0; x < 256 / 8; x++)
            {
                const uint8_t pixels = px[y % 2];
                const uint8_t attribute = zxColors[(y / 8) * 32 + x];
                PutEightPixels(&startLine[x * 8], pixels, attribute);
            }
        }
    }

    void LoadMcX(const uint8_t* buffer, sBitmapDescription& desc, const ZXProperty& prop)
    {
        const uint8_t* zxPixels = buffer;
        const uint8_t* zxColors = buffer + 6144;

        if (prop.type == ZXProperty::Type::Mc1)
        {
            const uint32_t blockHeight = 1;

            auto out = (sPixelRGB*)desc.bitmap.data();
            FillLinear(zxPixels, zxColors, blockHeight, desc.width, out);
        }
        else
        {
            const uint32_t blockHeight = prop.type == ZXProperty::Type::Mc2 ? 2 : 4;

            for (uint32_t i = 0; i < 3; i++)
            {
                auto out = (sPixelRGB*)(desc.bitmap.data() + desc.pitch * 64 * i);
                FillThird(i, zxPixels, zxColors, desc, blockHeight, out);
            }
        }
    }

    void LoadBMc4(const uint8_t* buffer, sBitmapDescription& desc, const ZXProperty& prop)
    {
        for (uint32_t i = 0; i < 3; i++)
        {
            auto out = (sPixelRGB*)(desc.bitmap.data() + ((prop.dy + 64 * i) * desc.pitch)) + prop.dx;
            auto zxPixels = buffer + 2048 * i;

            auto zxColors0 = buffer + 6144 + (768 / 3) * i;
            auto zxColors1 = buffer + 6144 + 768 + (768 / 3) * i;

            for (uint32_t y = 0; y < 64; y++)
            {
                const uint32_t line = (y * 8) % 64 + (y * 8) / 64;
                auto startLine = &out[line * desc.width];
                auto zxColors = ((line % 8) < 4 ? zxColors0 : zxColors1) + (line / 8) * 32;
                for (uint32_t x = 0; x < 256 / 8; x++)
                {
                    const uint8_t pixels = *zxPixels++;
                    const uint8_t attribute = *zxColors++;
                    PutEightPixels(&startLine[x * 8], pixels, attribute);
                }
            }
        }

        const uint8_t* zxBorder = buffer + 6144 + 768 * 2;
        MakeBorder(desc, zxBorder);
    }

    void LoadImg(const uint8_t* buffer, sBitmapDescription& desc, const ZXProperty& prop)
    {
        const uint32_t blockHeight = 8;
        const uint8_t* zxPixels = buffer;
        const uint8_t* zxColors = buffer + 6144;

        for (uint32_t i = 0; i < 3; i++)
        {
            auto out = (sPixelRGB*)(desc.bitmap.data() + ((prop.dy + 64 * i) * desc.pitch)) + prop.dx;
            FillThird(i, zxPixels, zxColors, desc, blockHeight, out);
        }
    }

    void LoadMgh(const uint8_t* buffer, sBitmapDescription& desc, const ZXProperty& prop)
    {
        const uint32_t blockHeight = buffer[4];

        const auto border = MergeColors(buffer[5], buffer[6]);
        MakeBorder(desc, border);

        buffer += 256; // skip header

        const uint8_t* zxPixels[2] = { buffer, buffer + 6144 };
        const uint8_t* zxColors[2] = { buffer + 6144 * 2, buffer + 6144 * 2 + 768 };

        for (uint32_t i = 0; i < 3; i++)
        {
            auto out = (sPixelRGB*)(desc.bitmap.data() + ((prop.dy + 64 * i) * desc.pitch)) + prop.dx;
            FillThird(i, zxPixels[0], zxPixels[1], zxColors[0], zxColors[1], desc, blockHeight, out);
        }
    }

    void LoadMgs(const uint8_t* buffer, sBitmapDescription& desc, const ZXProperty& prop)
    {
        const uint32_t blockHeight = buffer[4];

        const auto border = MergeColors(buffer[5], buffer[6]);
        MakeBorder(desc, border);

        buffer += 7; // skip header

        const uint8_t* zxPixels = buffer;
        const uint8_t* zxColors = buffer + 6144 * 2;

        auto out = (sPixelRGB*)(desc.bitmap.data() + prop.dy * desc.pitch) + prop.dx;

        for (uint32_t y = 0; y < 192; y++)
        {
            auto startLine = &out[y * desc.width];
            auto colors = &zxColors[(y / blockHeight) * 64];
            for (uint32_t x = 0; x < 256 / 8; x++)
            {
                const uint8_t pixels = *zxPixels++;
                const uint8_t ink = colors[x * 2 + 0];
                const uint8_t paper = colors[x * 2 + 1];
                const uint8_t attribute = ink | (paper << 3); // zxColors[(y / blockHeight) * 32 + x];
                PutEightPixels(&startLine[x * 8], pixels, attribute);
            }
        }
    }

} // namespace

bool cFormatScr::isSupported(cFile& file, Buffer& buffer) const
{
    if (readBuffer(file, buffer, 4) == false)
    {
        return false;
    }

    const auto prop = GetType(file.getSize(), buffer.data());
    return prop.type != ZXProperty::Type::Unknown;
}

bool cFormatScr::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    cFile file;
    if (openFile(file, filename, desc) == false)
    {
        return false;
    }

    const auto size = file.getSize();

    std::vector<uint8_t> buffer(size);
    if (file.read(buffer.data(), size) != size)
    {
        cLog::Error("Can't read ZX-Spectrum screen data.");
        return false;
    }

    const auto prop = GetType(size, buffer.data());
    if (prop.type == ZXProperty::Type::Unknown)
    {
        cLog::Error("Not a ZX-Spectrum screen.");
        return false;
    }

    desc.bppImage = 1;
    desc.bpp = 24;
    desc.format = ePixelFormat::RGB;

    desc.width = prop.cw;
    desc.height = prop.ch;
    desc.allocate(desc.width, desc.height, 24, ePixelFormat::RGB);

    desc.formatName = prop.formatName;

    switch (prop.type)
    {
    case ZXProperty::Type::Scr:
    case ZXProperty::Type::ScS:
        LoadScr(buffer.data(), desc, prop);
        break;

    case ZXProperty::Type::Bsc:
        LoadBsc(buffer.data(), desc, prop);
        break;

    case ZXProperty::Type::Atr:
        LoadAtr(buffer.data(), desc, prop);
        break;

    case ZXProperty::Type::Mc1:
    case ZXProperty::Type::Mc2:
    case ZXProperty::Type::Mc4:
        LoadMcX(buffer.data(), desc, prop);
        break;

    case ZXProperty::Type::BMc4:
        LoadBMc4(buffer.data(), desc, prop);
        break;

    case ZXProperty::Type::Img:
        LoadImg(buffer.data(), desc, prop);
        break;

    case ZXProperty::Type::Mgh:
        LoadMgh(buffer.data(), desc, prop);
        break;

    case ZXProperty::Type::Mgs:
        LoadMgs(buffer.data(), desc, prop);
        break;

    case ZXProperty::Type::Unknown: // prevent compiler warning
        break;
    }

    return true;
}

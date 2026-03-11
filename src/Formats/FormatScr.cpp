/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatScr.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
#include "Log/Log.h"

#include <cstring>

namespace
{
    struct Color
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    // palette PULSAR (0xcd)
    constexpr Color Palette[] = {
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

    struct PixelRGB
    {
        void set(bool isSet, uint8_t attribute)
        {
            const auto bright = (attribute & 0x40) >> 3;
            if (isSet == false)
            {
                attribute >>= 3;
            }
            color = Palette[bright + (attribute & 0x07)];
        }

        Color color;
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
        static constexpr struct
        {
            uint32_t size;
            ZXProperty prop;
        } SizeList[] = {
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

        for (const auto& s : SizeList)
        {
            if (s.size == fileSize)
            {
                return s.prop;
            }
        }

        if (buffer[0] == 'M' && buffer[1] == 'G')
        {
            const auto blockHeight = buffer[4];
            if (buffer[2] == 'H' && (fileSize == 19456 || fileSize == 18688 || fileSize == 15616 || fileSize == 14080))
            {
                // 19456 - mg1
                // 18688 - mg2
                // 15616 - mg4
                // 14080 - mg8
                static char formatName[20];
                ::snprintf(formatName, sizeof(formatName), "zx-mgh%u", blockHeight);
                return { 320, 240, 256, 192, 32, 24, ZXProperty::Type::Mgh, formatName };
            }
            else if (buffer[2] == 'S' && fileSize == 36871)
            {
                // 36871 - mgs
                static char formatName[20];
                ::snprintf(formatName, sizeof(formatName), "zx-mgs%u", blockHeight);
                return { 320, 240, 256, 192, 32, 24, ZXProperty::Type::Mgs, formatName };
            }
        }

        return { 0, 0, 0, 0, 0, 0, ZXProperty::Type::Unknown, "" };
    }

    void PutSixteenPixels(PixelRGB* out, uint8_t color)
    {
        const auto left = static_cast<uint8_t>(color & 0x07);
        for (uint32_t i = 0; i < 8; i++)
        {
            out->set(true, left);
            out++;
        }

        const auto right = static_cast<uint8_t>((color >> 3) & 0x07);
        for (uint32_t i = 0; i < 8; i++)
        {
            out->set(true, right);
            out++;
        }
    }

    Color GetColorIntensity(uint8_t attr)
    {
        auto b = static_cast<uint8_t>((attr & 1) ? 1 : 0);
        auto r = static_cast<uint8_t>((attr & 2) ? 1 : 0);
        auto g = static_cast<uint8_t>((attr & 4) ? 1 : 0);

        return { r, g, b };
    }

    Color MergeColors(uint8_t attr0, uint8_t attr1)
    {
        const auto c0 = GetColorIntensity(attr0);
        const auto c1 = GetColorIntensity(attr1);

        const bool b0 = (attr0 & 0x40) != 0;
        const bool b1 = (attr1 & 0x40) != 0;

        const bool z0 = (attr0 & 0x07) == 0;
        const bool z1 = (attr1 & 0x07) == 0;

        const bool n0 = z0 == false && b0 == false;
        const bool n1 = z1 == false && b1 == false;

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
        static constexpr uint8_t Intensity[] = { 0x00, 0x76, 0xcd, 0xe9, 0xff, 0x9f };
        // 0x00 - ZZ - zero + zero
        // 0x76 - NN - normal + normal
        // 0xcd - BB - bright + bright
        // 0xe9 - ZN - zero + normal
        // 0xff - NB - normal + bright
        // 0x9f - ZB - zero + bright

        const auto i = Intensity[idx];

        constexpr float V0 = 0.5f;
        constexpr float V1 = 1.0f - V0;
        return {
            static_cast<uint8_t>(V0 * i * c0.r + V1 * i * c1.r),
            static_cast<uint8_t>(V0 * i * c0.g + V1 * i * c1.g),
            static_cast<uint8_t>(V0 * i * c0.b + V1 * i * c1.b),
        };
    }

    Color MergeColors(const Color& c0, const Color& c1)
    {
        constexpr float A = 0.5f;
        constexpr float B = 1.0f - A;
        return {
            static_cast<uint8_t>(c0.r * A + c1.r * B),
            static_cast<uint8_t>(c0.g * A + c1.g * B),
            static_cast<uint8_t>(c0.b * A + c1.b * B),
        };
    }

    void MakeBorder(sChunkData& chunk, const Color& color)
    {
        auto pixel = reinterpret_cast<PixelRGB*>(chunk.bitmap.data());

        for (uint32_t i = 0, size = chunk.width * chunk.height; i < size; i++)
        {
            pixel[i].color = color;
        }
    }

    void MakeBorder(sChunkData& chunk, const uint8_t* zxBorder)
    {
        // top
        for (uint32_t y = 0; y < 64; y++)
        {
            auto out = reinterpret_cast<PixelRGB*>(chunk.bitmap.data() + y * chunk.pitch);
            for (uint32_t x = 0; x < 24; x++)
            {
                const auto color = *zxBorder++;
                PutSixteenPixels(out, color);
                out += 16;
            }
        }

        // left / right
        for (uint32_t y = 0; y < 192; y++)
        {
            auto out = reinterpret_cast<PixelRGB*>(chunk.bitmap.data() + (y + 64) * chunk.pitch);
            for (uint32_t x = 0; x < 4; x++)
            {
                const auto color = *zxBorder++;
                PutSixteenPixels(out, color);
                out += 16;
            }

            out += 256;
            for (uint32_t x = 0; x < 4; x++)
            {
                const auto color = *zxBorder++;
                PutSixteenPixels(out, color);
                out += 16;
            }
        }

        // bottom
        for (uint32_t y = 0; y < 48; y++)
        {
            auto out = reinterpret_cast<PixelRGB*>(chunk.bitmap.data() + (y + 64 + 192) * chunk.pitch);
            for (uint32_t x = 0; x < 24; x++)
            {
                const auto color = *zxBorder++;
                PutSixteenPixels(out, color);
                out += 16;
            }
        }
    }

    void PutEightPixels(PixelRGB* out, const uint8_t pixels, const uint8_t attribute)
    {
        for (uint32_t i = 0; i < 8; i++)
        {
            const auto bit = 0x80 >> i;
            const bool isSet = pixels & bit;
            out->set(isSet, attribute);
            out++;
        }
    }

    void PutEightPixels(PixelRGB* out, const uint8_t pixels[2], const uint8_t attributes[2])
    {
        for (uint32_t i = 0; i < 8; i++)
        {
            const auto bit = 0x80 >> i;

            PixelRGB pixel0;
            pixel0.set(pixels[0] & bit, attributes[0]);

            PixelRGB pixel1;
            pixel1.set(pixels[1] & bit, attributes[1]);

            out->color = MergeColors(pixel0.color, pixel1.color);
            out++;
        }
    }

    void FillThird(uint32_t layer, const uint8_t* zxPixels, const uint8_t* zxColors,
                   sChunkData& chunk, uint32_t blockHeight, PixelRGB* out)
    {
        zxPixels += 2048 * layer;
        zxColors += 2048 / blockHeight * layer;

        for (uint32_t y = 0; y < 64; y++)
        {
            const auto line = (y * 8) % 64 + (y * 8) / 64;
            auto startLine = &out[line * chunk.width];
            for (uint32_t x = 0; x < 256 / 8; x++)
            {
                const auto pixels = *zxPixels++;
                const auto attribute = zxColors[(line / blockHeight) * 32 + x];
                PutEightPixels(&startLine[x * 8], pixels, attribute);
            }
        }
    }

    void FillThird(uint32_t layer, const uint8_t* zxPixels0, const uint8_t* zxPixels1,
                   const uint8_t* zxColors0, const uint8_t* zxColors1,
                   sChunkData& chunk, uint32_t blockHeight, PixelRGB* out)
    {
        zxPixels0 += 2048 * layer;
        zxPixels1 += 2048 * layer;

        zxColors0 += 2048 / blockHeight * layer;
        zxColors1 += 2048 / blockHeight * layer;

        for (uint32_t y = 0; y < 64; y++)
        {
            const auto line = (y * 8) % 64 + (y * 8) / 64;
            auto startLine = &out[line * chunk.width];
            for (uint32_t x = 0; x < 256 / 8; x++)
            {
                const uint8_t pixels[2] = { *zxPixels0++, *zxPixels1++ };

                const auto idx = (line / blockHeight) * 32 + x;
                const uint8_t attributes[2] = { zxColors0[idx], zxColors1[idx] };
                PutEightPixels(&startLine[x * 8], pixels, attributes);
            }
        }
    }

    void FillLinear(const uint8_t* zxPixels, const uint8_t* zxColors, uint32_t blockHeight, uint32_t outWidth, PixelRGB* out)
    {
        for (uint32_t y = 0; y < 192; y++)
        {
            auto startLine = &out[y * outWidth];
            for (uint32_t x = 0; x < 256 / 8; x++)
            {
                const auto pixels = *zxPixels++;
                const auto attribute = zxColors[(y / blockHeight) * 32 + x];
                PutEightPixels(&startLine[x * 8], pixels, attribute);
            }
        }
    }

    void LoadScr(const uint8_t* buffer, sChunkData& chunk, sImageInfo& info, const ZXProperty& prop)
    {
        if (prop.type == ZXProperty::Type::ScS)
        {
            info.exifList.push_back({ sImageInfo::ExifCategory::Info, "Comment", reinterpret_cast<const char*>(buffer) });
            buffer += 17;
        }
        const auto zxPixels = buffer;
        const auto zxColors = buffer + 6144;

        for (uint32_t i = 0; i < 3; i++)
        {
            auto out = reinterpret_cast<PixelRGB*>(chunk.bitmap.data() + (prop.dy + 64 * i) * chunk.pitch) + prop.dx;
            FillThird(i, zxPixels, zxColors, chunk, 8, out);
        }
    }

    void LoadBsc(const uint8_t* buffer, sChunkData& chunk, const ZXProperty& prop)
    {
        const auto zxPixels = buffer;
        const auto zxColors = buffer + 6144;

        for (uint32_t i = 0; i < 3; i++)
        {
            auto out = reinterpret_cast<PixelRGB*>(chunk.bitmap.data() + (prop.dy + 64 * i) * chunk.pitch) + prop.dx;
            FillThird(i, zxPixels, zxColors, chunk, 8, out);
        }

        const auto zxBorder = buffer + 6144 + 768;
        MakeBorder(chunk, zxBorder);
    }

    void LoadAtr(const uint8_t* buffer, sChunkData& chunk, const ZXProperty& prop)
    {
        const uint8_t px[2] = { 0x55, 0xaa };

        const auto zxColors = buffer;
        auto out = reinterpret_cast<PixelRGB*>(chunk.bitmap.data() + prop.dy * chunk.pitch) + prop.dx;

        for (uint32_t y = 0; y < 192; y++)
        {
            auto startLine = &out[y * 256];
            for (uint32_t x = 0; x < 256 / 8; x++)
            {
                const auto pixels = px[y % 2];
                const auto attribute = zxColors[(y / 8) * 32 + x];
                PutEightPixels(&startLine[x * 8], pixels, attribute);
            }
        }
    }

    void LoadMcX(const uint8_t* buffer, sChunkData& chunk, const ZXProperty& prop)
    {
        const auto zxPixels = buffer;
        const auto zxColors = buffer + 6144;

        if (prop.type == ZXProperty::Type::Mc1)
        {
            constexpr uint32_t BlockHeight = 1;

            auto out = reinterpret_cast<PixelRGB*>(chunk.bitmap.data());
            FillLinear(zxPixels, zxColors, BlockHeight, chunk.width, out);
        }
        else
        {
            const auto blockHeight = prop.type == ZXProperty::Type::Mc2 ? 2u : 4u;

            for (uint32_t i = 0; i < 3; i++)
            {
                auto out = reinterpret_cast<PixelRGB*>(chunk.bitmap.data() + chunk.pitch * 64 * i);
                FillThird(i, zxPixels, zxColors, chunk, blockHeight, out);
            }
        }
    }

    void LoadBMc4(const uint8_t* buffer, sChunkData& chunk, const ZXProperty& prop)
    {
        for (uint32_t i = 0; i < 3; i++)
        {
            auto out = reinterpret_cast<PixelRGB*>(chunk.bitmap.data() + (prop.dy + 64 * i) * chunk.pitch) + prop.dx;
            auto zxPixels = buffer + 2048 * i;

            auto zxColors0 = buffer + 6144 + (768 / 3) * i;
            auto zxColors1 = buffer + 6144 + 768 + (768 / 3) * i;

            for (uint32_t y = 0; y < 64; y++)
            {
                const auto line = (y * 8) % 64 + (y * 8) / 64;
                auto startLine = &out[line * chunk.width];
                auto zxColors = ((line % 8) < 4 ? zxColors0 : zxColors1) + (line / 8) * 32;
                for (uint32_t x = 0; x < 256 / 8; x++)
                {
                    const auto pixels = *zxPixels++;
                    const auto attribute = *zxColors++;
                    PutEightPixels(&startLine[x * 8], pixels, attribute);
                }
            }
        }

        const auto zxBorder = buffer + 6144 + 768 * 2;
        MakeBorder(chunk, zxBorder);
    }

    void LoadImg(const uint8_t* buffer, sChunkData& chunk, const ZXProperty& prop)
    {
        constexpr uint32_t BlockHeight = 8;
        const auto zxPixels = buffer;
        const auto zxColors = buffer + 6144;

        for (uint32_t i = 0; i < 3; i++)
        {
            auto out = reinterpret_cast<PixelRGB*>(chunk.bitmap.data() + (prop.dy + 64 * i) * chunk.pitch) + prop.dx;
            FillThird(i, zxPixels, zxColors, chunk, BlockHeight, out);
        }
    }

    void LoadMgh(const uint8_t* buffer, sChunkData& chunk, const ZXProperty& prop)
    {
        const uint32_t blockHeight = buffer[4];

        const auto border = MergeColors(buffer[5], buffer[6]);
        MakeBorder(chunk, border);

        buffer += 256; // skip header

        const uint8_t* zxPixels[2] = { buffer, buffer + 6144 };
        const uint8_t* zxColors[2] = { buffer + 6144 * 2, buffer + 6144 * 2 + 768 };

        for (uint32_t i = 0; i < 3; i++)
        {
            auto out = reinterpret_cast<PixelRGB*>(chunk.bitmap.data() + (prop.dy + 64 * i) * chunk.pitch) + prop.dx;
            FillThird(i, zxPixels[0], zxPixels[1], zxColors[0], zxColors[1], chunk, blockHeight, out);
        }
    }

    void LoadMgs(const uint8_t* buffer, sChunkData& chunk, const ZXProperty& prop)
    {
        const uint32_t blockHeight = buffer[4];

        const auto border = MergeColors(buffer[5], buffer[6]);
        MakeBorder(chunk, border);

        buffer += 7; // skip header

        auto zxPixels = buffer;
        auto zxColors = buffer + 6144 * 2;

        auto out = reinterpret_cast<PixelRGB*>(chunk.bitmap.data() + prop.dy * chunk.pitch) + prop.dx;

        for (uint32_t y = 0; y < 192; y++)
        {
            auto startLine = &out[y * chunk.width];
            auto colors = &zxColors[(y / blockHeight) * 64];
            for (uint32_t x = 0; x < 256 / 8; x++)
            {
                const auto pixels = *zxPixels++;
                const auto ink = colors[x * 2 + 0];
                const auto paper = colors[x * 2 + 1];
                const auto attribute = static_cast<uint8_t>(ink | (paper << 3));
                PutEightPixels(&startLine[x * 8], pixels, attribute);
            }
        }
    }

} // namespace

bool cFormatScr::isSupported(cFile& file, Buffer& buffer) const
{
    if (readBuffer(file, buffer, 7) == false)
    {
        return false;
    }

    const auto prop = GetType(file.getSize(), buffer.data());
    return prop.type != ZXProperty::Type::Unknown;
}

bool cFormatScr::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (openFile(file, filename, info) == false)
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

    info.bppImage = 1;
    chunk.allocate(prop.cw, prop.ch, 24, ePixelFormat::RGB);

    info.formatName = prop.formatName;

    switch (prop.type)
    {
    case ZXProperty::Type::Scr:
    case ZXProperty::Type::ScS:
        LoadScr(buffer.data(), chunk, info, prop);
        break;

    case ZXProperty::Type::Bsc:
        LoadBsc(buffer.data(), chunk, prop);
        break;

    case ZXProperty::Type::Atr:
        LoadAtr(buffer.data(), chunk, prop);
        break;

    case ZXProperty::Type::Mc1:
    case ZXProperty::Type::Mc2:
    case ZXProperty::Type::Mc4:
        LoadMcX(buffer.data(), chunk, prop);
        break;

    case ZXProperty::Type::BMc4:
        LoadBMc4(buffer.data(), chunk, prop);
        break;

    case ZXProperty::Type::Img:
        LoadImg(buffer.data(), chunk, prop);
        break;

    case ZXProperty::Type::Mgh:
        LoadMgh(buffer.data(), chunk, prop);
        break;

    case ZXProperty::Type::Mgs:
        LoadMgs(buffer.data(), chunk, prop);
        break;

    case ZXProperty::Type::Unknown: // prevent compiler warning
        break;
    }

    return true;
}

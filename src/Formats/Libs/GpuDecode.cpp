/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "GpuDecode.h"

#include <algorithm>
#include <astcenc/astcenc.h>
#include <cstring>

namespace
{
    struct Rgba
    {
        uint8_t r, g, b, a;
    };

    inline uint8_t clampByte(int v)
    {
        return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
    }

    void writePixel(uint8_t* dst, uint32_t x, uint32_t y, uint32_t width, uint32_t height, const Rgba& c)
    {
        if (x < width && y < height)
        {
            auto p = dst + (y * width + x) * 4;
            p[0] = c.r;
            p[1] = c.g;
            p[2] = c.b;
            p[3] = c.a;
        }
    }

    // -------------------------------------------------------------------------
    // BC1 (DXT1) decoder
    // -------------------------------------------------------------------------

    void decodeBC1Block(const uint8_t* src, Rgba pixels[16])
    {
        uint16_t c0 = src[0] | (src[1] << 8);
        uint16_t c1 = src[2] | (src[3] << 8);

        Rgba color[4];
        color[0] = { static_cast<uint8_t>(((c0 >> 11) & 0x1f) * 255 / 31),
                     static_cast<uint8_t>(((c0 >> 5) & 0x3f) * 255 / 63),
                     static_cast<uint8_t>(((c0 >> 0) & 0x1f) * 255 / 31), 255 };
        color[1] = { static_cast<uint8_t>(((c1 >> 11) & 0x1f) * 255 / 31),
                     static_cast<uint8_t>(((c1 >> 5) & 0x3f) * 255 / 63),
                     static_cast<uint8_t>(((c1 >> 0) & 0x1f) * 255 / 31), 255 };

        if (c0 > c1)
        {
            color[2] = { static_cast<uint8_t>((color[0].r * 2 + color[1].r) / 3),
                         static_cast<uint8_t>((color[0].g * 2 + color[1].g) / 3),
                         static_cast<uint8_t>((color[0].b * 2 + color[1].b) / 3), 255 };
            color[3] = { static_cast<uint8_t>((color[0].r + color[1].r * 2) / 3),
                         static_cast<uint8_t>((color[0].g + color[1].g * 2) / 3),
                         static_cast<uint8_t>((color[0].b + color[1].b * 2) / 3), 255 };
        }
        else
        {
            color[2] = { static_cast<uint8_t>((color[0].r + color[1].r) / 2),
                         static_cast<uint8_t>((color[0].g + color[1].g) / 2),
                         static_cast<uint8_t>((color[0].b + color[1].b) / 2), 255 };
            color[3] = { 0, 0, 0, 0 };
        }

        uint32_t indices = src[4] | (src[5] << 8) | (src[6] << 16) | (src[7] << 24);
        for (int i = 0; i < 16; i++)
        {
            pixels[i] = color[indices & 3];
            indices >>= 2;
        }
    }

    // -------------------------------------------------------------------------
    // BC3 (DXT5) decoder
    // -------------------------------------------------------------------------

    void decodeBC3Alpha(const uint8_t* src, uint8_t alphas[16])
    {
        uint8_t a0 = src[0];
        uint8_t a1 = src[1];

        uint8_t palette[8];
        palette[0] = a0;
        palette[1] = a1;
        if (a0 > a1)
        {
            for (int i = 1; i <= 6; i++)
            {
                palette[i + 1] = static_cast<uint8_t>(((7 - i) * a0 + i * a1) / 7);
            }
        }
        else
        {
            for (int i = 1; i <= 4; i++)
            {
                palette[i + 1] = static_cast<uint8_t>(((5 - i) * a0 + i * a1) / 5);
            }
            palette[6] = 0;
            palette[7] = 255;
        }

        uint64_t bits = 0;
        for (int i = 0; i < 6; i++)
        {
            bits |= static_cast<uint64_t>(src[2 + i]) << (8 * i);
        }

        for (int i = 0; i < 16; i++)
        {
            alphas[i] = palette[bits & 7];
            bits >>= 3;
        }
    }

    // -------------------------------------------------------------------------
    // BC7 decoder
    // -------------------------------------------------------------------------

    struct BitReader
    {
        const uint8_t* data;
        int pos;

        uint32_t read(int bits)
        {
            uint32_t val = 0;
            for (int i = 0; i < bits; i++)
            {
                int byteIdx = pos >> 3;
                int bitIdx = pos & 7;
                val |= ((data[byteIdx] >> bitIdx) & 1) << i;
                pos++;
            }
            return val;
        }
    };

    constexpr int Bc7Partitions2[64][16] = {
        { 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1 },
        { 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1 },
        { 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1 },
        { 0, 0, 0, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 1 },
        { 0, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1 },
        { 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1 },
        { 0, 0, 1, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1 },
        { 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1 },
        { 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1, 1 },
        { 0, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0 },
        { 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0 },
        { 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0 },
        { 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 1 },
        { 0, 0, 1, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 1, 0, 0 },
        { 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0 },
        { 0, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0, 0 },
        { 0, 0, 0, 1, 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0 },
        { 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
        { 0, 1, 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0 },
        { 0, 0, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0 },
        { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1 },
        { 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1 },
        { 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0 },
        { 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0 },
        { 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0 },
        { 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0 },
        { 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1 },
        { 0, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1 },
        { 0, 1, 1, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 1, 0 },
        { 0, 0, 0, 1, 0, 0, 1, 1, 1, 1, 0, 0, 1, 0, 0, 0 },
        { 0, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1, 0, 0 },
        { 0, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 0, 0 },
        { 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0, 1, 1, 0 },
        { 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 1, 1 },
        { 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1 },
        { 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0 },
        { 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0 },
        { 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0, 0, 0, 0 },
        { 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0 },
        { 0, 0, 0, 0, 0, 1, 0, 0, 1, 1, 1, 0, 0, 1, 0, 0 },
        { 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 1 },
        { 0, 0, 1, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 0, 0, 1 },
        { 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0 },
        { 0, 0, 1, 1, 1, 0, 0, 1, 1, 1, 0, 0, 0, 1, 1, 0 },
        { 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 0, 0, 1 },
        { 0, 1, 1, 0, 0, 0, 1, 1, 0, 0, 1, 1, 1, 0, 0, 1 },
        { 0, 1, 1, 1, 1, 1, 1, 0, 1, 0, 0, 0, 0, 0, 0, 1 },
        { 0, 0, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 0, 1, 1, 1 },
        { 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1 },
        { 0, 0, 1, 1, 0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0 },
        { 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 1, 0 },
        { 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1, 1, 1 },
    };

    constexpr int Bc7Partitions3[64][16] = {
        { 0, 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 1, 2, 2, 2, 2 },
        { 0, 0, 0, 1, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2, 2, 1 },
        { 0, 0, 0, 0, 2, 0, 0, 1, 2, 2, 1, 1, 2, 2, 1, 1 },
        { 0, 2, 2, 2, 0, 0, 2, 2, 0, 0, 1, 1, 0, 1, 1, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2 },
        { 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 2, 2, 0, 0, 2, 2 },
        { 0, 0, 2, 2, 0, 0, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1 },
        { 0, 0, 1, 1, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2 },
        { 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2 },
        { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2 },
        { 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2 },
        { 0, 1, 1, 2, 0, 1, 1, 2, 0, 1, 1, 2, 0, 1, 1, 2 },
        { 0, 1, 2, 2, 0, 1, 2, 2, 0, 1, 2, 2, 0, 1, 2, 2 },
        { 0, 0, 1, 1, 0, 1, 1, 2, 1, 1, 2, 2, 1, 2, 2, 2 },
        { 0, 0, 1, 1, 2, 0, 0, 1, 2, 2, 0, 0, 2, 2, 2, 0 },
        { 0, 0, 0, 1, 0, 0, 1, 1, 0, 1, 1, 2, 1, 1, 2, 2 },
        { 0, 1, 1, 1, 0, 0, 1, 1, 2, 0, 0, 1, 2, 2, 0, 0 },
        { 0, 0, 0, 0, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2 },
        { 0, 0, 2, 2, 0, 0, 2, 2, 0, 0, 2, 2, 1, 1, 1, 1 },
        { 0, 1, 1, 1, 0, 1, 1, 1, 0, 2, 2, 2, 0, 2, 2, 2 },
        { 0, 0, 0, 1, 0, 0, 0, 1, 2, 2, 2, 1, 2, 2, 2, 1 },
        { 0, 0, 0, 0, 0, 0, 1, 1, 0, 1, 2, 2, 0, 1, 2, 2 },
        { 0, 0, 0, 0, 1, 1, 0, 0, 2, 2, 1, 0, 2, 2, 1, 0 },
        { 0, 1, 2, 2, 0, 1, 2, 2, 0, 0, 1, 1, 0, 0, 0, 0 },
        { 0, 0, 1, 2, 0, 0, 1, 2, 1, 1, 2, 2, 2, 2, 2, 2 },
        { 0, 1, 1, 0, 1, 2, 2, 1, 1, 2, 2, 1, 0, 1, 1, 0 },
        { 0, 0, 0, 0, 0, 1, 1, 0, 1, 2, 2, 1, 1, 2, 2, 1 },
        { 0, 0, 2, 2, 1, 1, 0, 2, 1, 1, 0, 2, 0, 0, 2, 2 },
        { 0, 1, 1, 0, 0, 1, 1, 0, 2, 0, 0, 2, 2, 2, 2, 2 },
        { 0, 0, 1, 1, 0, 1, 2, 2, 0, 1, 2, 2, 0, 0, 1, 1 },
        { 0, 0, 0, 0, 2, 0, 0, 0, 2, 2, 1, 1, 2, 2, 2, 1 },
        { 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 2, 1, 2, 2, 2 },
        { 0, 2, 2, 2, 0, 0, 2, 2, 0, 0, 1, 2, 0, 0, 1, 1 },
        { 0, 0, 1, 1, 0, 0, 1, 2, 0, 0, 2, 2, 0, 2, 2, 2 },
        { 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0, 0, 1, 2, 0 },
        { 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0 },
        { 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0, 1, 2, 0 },
        { 0, 1, 2, 0, 2, 0, 1, 2, 1, 2, 0, 1, 0, 1, 2, 0 },
        { 0, 0, 1, 1, 2, 2, 0, 0, 1, 1, 2, 2, 0, 0, 1, 1 },
        { 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 0, 0, 0, 0, 1, 1 },
        { 0, 1, 0, 1, 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 2, 1, 2, 1, 2, 1 },
        { 0, 0, 2, 2, 1, 1, 2, 2, 0, 0, 2, 2, 1, 1, 2, 2 },
        { 0, 0, 2, 2, 0, 0, 1, 1, 0, 0, 2, 2, 0, 0, 1, 1 },
        { 0, 2, 2, 0, 1, 2, 2, 1, 0, 2, 2, 0, 1, 2, 2, 1 },
        { 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 0, 1, 0, 1 },
        { 0, 0, 0, 0, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1 },
        { 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 2, 2, 2 },
        { 0, 2, 2, 2, 0, 1, 1, 1, 0, 2, 2, 2, 0, 1, 1, 1 },
        { 0, 0, 0, 2, 1, 1, 1, 2, 0, 0, 0, 2, 1, 1, 1, 2 },
        { 0, 0, 0, 0, 2, 1, 1, 2, 2, 1, 1, 2, 2, 1, 1, 2 },
        { 0, 2, 2, 2, 0, 1, 1, 1, 0, 1, 1, 1, 0, 2, 2, 2 },
        { 0, 0, 0, 2, 1, 1, 1, 2, 1, 1, 1, 2, 0, 0, 0, 2 },
        { 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 2, 2 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2, 2, 1, 1, 2 },
        { 0, 1, 1, 0, 0, 1, 1, 0, 2, 2, 2, 2, 2, 2, 2, 2 },
        { 0, 0, 2, 2, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 2, 2 },
        { 0, 0, 2, 2, 1, 1, 2, 2, 1, 1, 2, 2, 0, 0, 2, 2 },
        { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 1, 2 },
        { 0, 0, 0, 2, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 1 },
        { 0, 2, 2, 2, 1, 2, 2, 2, 0, 2, 2, 2, 1, 2, 2, 2 },
        { 0, 1, 0, 1, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2 },
        { 0, 1, 1, 1, 2, 0, 1, 1, 2, 2, 0, 1, 2, 2, 2, 0 },
    };

    // anchor indices for 2 subsets (second anchor)
    constexpr int Bc7Anchor2[64] = {
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        2,
        8,
        2,
        2,
        8,
        8,
        15,
        2,
        8,
        2,
        2,
        8,
        8,
        2,
        2,
        15,
        15,
        6,
        8,
        2,
        8,
        15,
        15,
        2,
        8,
        2,
        2,
        2,
        15,
        15,
        6,
        6,
        2,
        6,
        8,
        15,
        15,
        2,
        2,
        15,
        15,
        15,
        15,
        3,
        8,
        15,
        15,
    };

    // anchor indices for 3 subsets (second and third anchors)
    constexpr int Bc7Anchor3a[64] = {
        3,
        3,
        15,
        15,
        8,
        3,
        15,
        15,
        8,
        8,
        6,
        6,
        6,
        5,
        3,
        3,
        3,
        3,
        8,
        15,
        3,
        3,
        6,
        10,
        5,
        8,
        8,
        6,
        8,
        5,
        15,
        15,
        8,
        15,
        3,
        5,
        6,
        10,
        8,
        15,
        15,
        3,
        15,
        5,
        15,
        15,
        15,
        15,
        3,
        15,
        5,
        5,
        5,
        8,
        5,
        10,
        5,
        10,
        8,
        13,
        15,
        12,
        3,
        3,
    };
    constexpr int Bc7Anchor3b[64] = {
        15,
        8,
        8,
        3,
        15,
        15,
        3,
        8,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        8,
        15,
        8,
        15,
        3,
        15,
        8,
        15,
        8,
        3,
        15,
        6,
        10,
        15,
        15,
        10,
        8,
        15,
        3,
        15,
        10,
        10,
        8,
        9,
        10,
        6,
        15,
        8,
        15,
        3,
        6,
        6,
        8,
        15,
        3,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        15,
        3,
        15,
        15,
        8,
    };

    // mode info: numSubsets, partBits, rotBits, idxSelBit, colorBits, alphaBits, pBits, idxBits, idx2Bits
    struct BC7ModeInfo
    {
        int ns, pb, rb, isb, cb, ab, epb, spb, ib, ib2;
    };
    constexpr BC7ModeInfo Bc7Modes[8] = {
        { 3, 4, 0, 0, 4, 0, 1, 0, 3, 0 }, // mode 0
        { 2, 6, 0, 0, 6, 0, 0, 1, 3, 0 }, // mode 1
        { 3, 6, 0, 0, 5, 0, 0, 0, 2, 0 }, // mode 2
        { 2, 6, 0, 0, 7, 0, 1, 0, 2, 0 }, // mode 3
        { 1, 0, 2, 1, 5, 6, 0, 0, 2, 3 }, // mode 4
        { 1, 0, 2, 0, 7, 8, 0, 0, 2, 2 }, // mode 5
        { 1, 0, 0, 0, 7, 7, 1, 0, 4, 0 }, // mode 6
        { 2, 6, 0, 0, 5, 5, 1, 0, 2, 0 }, // mode 7
    };

    void decodeBC7Block(const uint8_t* src, Rgba pixels[16])
    {
        BitReader br = { src, 0 };

        int mode = 0;
        while (mode < 8 && br.read(1) == 0)
        {
            mode++;
        }

        if (mode >= 8)
        {
            ::memset(pixels, 0, sizeof(Rgba) * 16);
            return;
        }

        const auto& mi = Bc7Modes[mode];

        uint32_t partition = br.read(mi.pb);
        uint32_t rotation = br.read(mi.rb);
        uint32_t idxSel = br.read(mi.isb);

        // read endpoints
        uint8_t endpoints[6][4] = {}; // [subset*2][rgba]
        int numEndpoints = mi.ns * 2;

        // color
        for (int c = 0; c < 3; c++)
        {
            for (int e = 0; e < numEndpoints; e++)
            {
                endpoints[e][c] = br.read(mi.cb);
            }
        }

        // alpha
        for (int e = 0; e < numEndpoints; e++)
        {
            endpoints[e][3] = mi.ab ? br.read(mi.ab) : 255;
        }

        // p-bits
        if (mi.epb)
        {
            for (int e = 0; e < numEndpoints; e++)
            {
                uint32_t pbit = br.read(1);
                for (int c = 0; c < (mi.ab ? 4 : 3); c++)
                {
                    endpoints[e][c] = static_cast<uint8_t>((endpoints[e][c] << 1) | pbit);
                }
            }
        }
        else if (mi.spb)
        {
            for (int s = 0; s < mi.ns; s++)
            {
                uint32_t pbit = br.read(1);
                for (int c = 0; c < (mi.ab ? 4 : 3); c++)
                {
                    endpoints[s * 2 + 0][c] = static_cast<uint8_t>((endpoints[s * 2 + 0][c] << 1) | pbit);
                    endpoints[s * 2 + 1][c] = static_cast<uint8_t>((endpoints[s * 2 + 1][c] << 1) | pbit);
                }
            }
        }

        // expand endpoints to 8 bits
        int colorPrec = mi.cb + (mi.epb || mi.spb ? 1 : 0);
        int alphaPrec = mi.ab + (mi.epb ? 1 : 0) + (mi.spb && mi.ab ? 1 : 0);
        for (int e = 0; e < numEndpoints; e++)
        {
            for (int c = 0; c < 3; c++)
            {
                endpoints[e][c] = static_cast<uint8_t>((endpoints[e][c] << (8 - colorPrec)) | (endpoints[e][c] >> (2 * colorPrec - 8)));
            }
            if (mi.ab)
            {
                endpoints[e][3] = static_cast<uint8_t>((endpoints[e][3] << (8 - alphaPrec)) | (endpoints[e][3] >> (2 * alphaPrec - 8)));
            }
        }

        // read index data
        uint8_t indices[16] = {};
        uint8_t indices2[16] = {};

        int ib1 = mi.ib;
        int ib2 = mi.ib2;

        for (int i = 0; i < 16; i++)
        {
            bool isAnchor = (i == 0);
            if (mi.ns == 2)
            {
                isAnchor = isAnchor || (i == Bc7Anchor2[partition]);
            }
            else if (mi.ns == 3)
            {
                isAnchor = isAnchor || (i == Bc7Anchor3a[partition]) || (i == Bc7Anchor3b[partition]);
            }
            indices[i] = br.read(isAnchor ? ib1 - 1 : ib1);
        }

        if (ib2)
        {
            for (int i = 0; i < 16; i++)
            {
                indices2[i] = br.read(i == 0 ? ib2 - 1 : ib2);
            }
        }

        // interpolate
        auto getWeight = [](int bits, int index) -> uint8_t {
            static constexpr uint8_t Bc7Weights2[] = { 0, 21, 43, 64 };
            static constexpr uint8_t Bc7Weights3[] = { 0, 9, 18, 27, 37, 46, 55, 64 };
            static constexpr uint8_t Bc7Weights4[] = { 0, 4, 9, 13, 17, 21, 26, 30, 34, 38, 43, 47, 51, 55, 60, 64 };

            switch (bits)
            {
            case 2:
                return Bc7Weights2[index];
            case 3:
                return Bc7Weights3[index];
            case 4:
                return Bc7Weights4[index];
            }

            return 0;
        };

        for (int i = 0; i < 16; i++)
        {
            int subset = 0;
            if (mi.ns == 2)
            {
                subset = Bc7Partitions2[partition][i];
            }
            else if (mi.ns == 3)
            {
                subset = Bc7Partitions3[partition][i];
            }

            auto& e0 = endpoints[subset * 2];
            auto& e1 = endpoints[subset * 2 + 1];

            uint8_t cw, aw;
            if (ib2 == 0)
            {
                cw = aw = getWeight(ib1, indices[i]);
            }
            else if (idxSel == 0)
            {
                cw = getWeight(ib1, indices[i]);
                aw = getWeight(ib2, indices2[i]);
            }
            else
            {
                aw = getWeight(ib1, indices[i]);
                cw = getWeight(ib2, indices2[i]);
            }

            pixels[i].r = static_cast<uint8_t>(((64 - cw) * e0[0] + cw * e1[0] + 32) >> 6);
            pixels[i].g = static_cast<uint8_t>(((64 - cw) * e0[1] + cw * e1[1] + 32) >> 6);
            pixels[i].b = static_cast<uint8_t>(((64 - cw) * e0[2] + cw * e1[2] + 32) >> 6);
            pixels[i].a = static_cast<uint8_t>(((64 - aw) * e0[3] + aw * e1[3] + 32) >> 6);

            if (rotation)
            {
                if (rotation == 1)
                    std::swap(pixels[i].a, pixels[i].r);
                else if (rotation == 2)
                    std::swap(pixels[i].a, pixels[i].g);
                else if (rotation == 3)
                    std::swap(pixels[i].a, pixels[i].b);
            }
        }
    }

    // -------------------------------------------------------------------------
    // ETC2 decoder
    // -------------------------------------------------------------------------

    constexpr int Etc2Modifier[8][2] = {
        { 2, 8 }, { 5, 17 }, { 9, 29 }, { 13, 42 }, { 18, 60 }, { 24, 80 }, { 33, 106 }, { 47, 183 }
    };

    constexpr int Etc2DistanceTable[8] = { 3, 6, 11, 16, 23, 32, 41, 64 };

    // EAC modifier table (Khronos spec Table C.12)
    constexpr int EacModifier[16][8] = {
        { -3, -6, -9, -15, 2, 5, 8, 14 },
        { -3, -7, -10, -13, 2, 6, 9, 12 },
        { -2, -5, -8, -13, 1, 4, 7, 12 },
        { -2, -4, -6, -13, 1, 3, 5, 12 },
        { -3, -6, -8, -12, 2, 5, 7, 11 },
        { -3, -7, -9, -11, 2, 6, 8, 10 },
        { -4, -7, -8, -11, 3, 6, 7, 10 },
        { -3, -5, -8, -11, 2, 4, 7, 10 },
        { -2, -6, -8, -10, 1, 5, 7, 9 },
        { -2, -5, -8, -10, 1, 4, 7, 9 },
        { -2, -4, -8, -10, 1, 3, 7, 9 },
        { -2, -5, -7, -10, 1, 4, 6, 9 },
        { -3, -4, -7, -10, 2, 3, 6, 9 },
        { -1, -2, -3, -10, 0, 1, 2, 9 },
        { -4, -6, -8, -9, 3, 5, 7, 8 },
        { -3, -5, -7, -9, 2, 4, 6, 8 },
    };

    void decodeEACAlpha(const uint8_t* src, uint8_t alphas[16])
    {
        int base = src[0];
        int multiplier = (src[1] >> 4) & 0x0f;
        int tableIdx = src[1] & 0x0f;

        // 48 bits of 3-bit indices in bytes 2-7 (big-endian)
        uint64_t bits = 0;
        for (int i = 0; i < 6; i++)
        {
            bits = (bits << 8) | src[2 + i];
        }

        // pixel at (x,y) uses 3-bit index starting at bit 45 - 3*(x*4+y), column-major
        for (int x = 0; x < 4; x++)
        {
            for (int y = 0; y < 4; y++)
            {
                int pixelIdx = x * 4 + y;
                int shift = 45 - 3 * pixelIdx;
                int idx = (bits >> shift) & 7;
                int modifier = EacModifier[tableIdx][idx];
                int value = base + modifier * multiplier;
                alphas[y * 4 + x] = clampByte(value);
            }
        }
    }

    void decodeETC2Block_RGB(const uint8_t* src, Rgba pixels[16])
    {
        uint64_t block = 0;
        for (int i = 0; i < 8; i++)
        {
            block = (block << 8) | src[i];
        }

        bool diffBit = (block >> 33) & 1;
        bool flipBit = (block >> 32) & 1;

        int baseR[2], baseG[2], baseB[2];

        bool tMode = false;
        bool hMode = false;
        bool planar = false;

        if (diffBit)
        {
            int r = (block >> 59) & 0x1f;
            int dr = (block >> 56) & 0x07;
            if (dr >= 4)
                dr -= 8;
            int rr = r + dr;
            if (rr < 0 || rr > 31)
            {
                tMode = true;
            }

            int g = (block >> 51) & 0x1f;
            int dg = (block >> 48) & 0x07;
            if (dg >= 4)
                dg -= 8;
            int gg = g + dg;
            if (!tMode && (gg < 0 || gg > 31))
            {
                hMode = true;
            }

            int b = (block >> 43) & 0x1f;
            int db = (block >> 40) & 0x07;
            if (db >= 4)
                db -= 8;
            int bb = b + db;
            if (!tMode && !hMode && (bb < 0 || bb > 31))
            {
                planar = true;
            }

            if (!tMode && !hMode && !planar)
            {
                baseR[0] = (r << 3) | (r >> 2);
                baseG[0] = (g << 3) | (g >> 2);
                baseB[0] = (b << 3) | (b >> 2);
                baseR[1] = (rr << 3) | (rr >> 2);
                baseG[1] = (gg << 3) | (gg >> 2);
                baseB[1] = (bb << 3) | (bb >> 2);
            }
        }
        else
        {
            baseR[0] = ((block >> 60) & 0xf) * 17;
            baseG[0] = ((block >> 52) & 0xf) * 17;
            baseB[0] = ((block >> 44) & 0xf) * 17;
            baseR[1] = ((block >> 56) & 0xf) * 17;
            baseG[1] = ((block >> 48) & 0xf) * 17;
            baseB[1] = ((block >> 40) & 0xf) * 17;
        }

        if (tMode)
        {
            int r1 = ((src[0] >> 1) & 0x0c) | (src[0] & 0x03);
            r1 = (r1 << 4) | r1;
            int g1 = (src[1] >> 4) & 0x0f;
            g1 = (g1 << 4) | g1;
            int b1 = src[1] & 0x0f;
            b1 = (b1 << 4) | b1;
            int r2 = (src[2] >> 4) & 0x0f;
            r2 = (r2 << 4) | r2;
            int g2 = (src[2]) & 0x0f;
            g2 = (g2 << 4) | g2;
            int b2 = (src[3] >> 4) & 0x0f;
            b2 = (b2 << 4) | b2;
            int dIdx = ((src[3] >> 1) & 6) | (src[3] & 1);
            int d = Etc2DistanceTable[dIdx];

            Rgba paint[4];
            paint[0] = { static_cast<uint8_t>(r1), static_cast<uint8_t>(g1), static_cast<uint8_t>(b1), 255 };
            paint[1] = { clampByte(r2 + d), clampByte(g2 + d), clampByte(b2 + d), 255 };
            paint[2] = { static_cast<uint8_t>(r2), static_cast<uint8_t>(g2), static_cast<uint8_t>(b2), 255 };
            paint[3] = { clampByte(r2 - d), clampByte(g2 - d), clampByte(b2 - d), 255 };

            for (int i = 0; i < 16; i++)
            {
                int col = i >> 2, row = i & 3;
                int bitIdx = col * 4 + row;
                int msb = (src[4 + (bitIdx >> 3)] >> (7 - (bitIdx & 7))) & 1;
                int lsb = (src[4 + ((bitIdx + 16) >> 3)] >> (7 - ((bitIdx + 16) & 7))) & 1;
                // Note: the mapping for T-mode: msb | lsb << 1
                pixels[row * 4 + col] = paint[msb | (lsb << 1)];
            }
            return;
        }

        if (hMode)
        {
            int r1 = (src[0] >> 3) & 0x0f;
            r1 |= (r1 << 4);
            int g1 = ((src[0] & 7) << 1) | ((src[1] >> 4) & 1);
            g1 |= (g1 << 4);
            int b1 = ((src[1] & 8) << 0) | ((src[1] >> 0) & 3) | ((src[2] >> 5) & 4);
            b1 = (b1 << 4) | b1;
            int r2 = (src[2] >> 3) & 0x0f;
            r2 |= (r2 << 4);
            int g2 = ((src[2] & 7) << 1) | (src[3] >> 7);
            g2 |= (g2 << 4);
            int b2 = (src[3] >> 3) & 0x0f;
            b2 |= (b2 << 4);
            int dIdx = (src[3] & 4) | ((src[3] & 1) << 1);
            int orderBit = ((r1 << 16) + (g1 << 8) + b1) >= ((r2 << 16) + (g2 << 8) + b2) ? 1 : 0;
            dIdx |= orderBit;
            int d = Etc2DistanceTable[dIdx];

            Rgba paint[4];
            paint[0] = { clampByte(r1 + d), clampByte(g1 + d), clampByte(b1 + d), 255 };
            paint[1] = { clampByte(r1 - d), clampByte(g1 - d), clampByte(b1 - d), 255 };
            paint[2] = { clampByte(r2 + d), clampByte(g2 + d), clampByte(b2 + d), 255 };
            paint[3] = { clampByte(r2 - d), clampByte(g2 - d), clampByte(b2 - d), 255 };

            for (int i = 0; i < 16; i++)
            {
                int col = i >> 2, row = i & 3;
                int bitIdx = col * 4 + row;
                int msb = (src[4 + (bitIdx >> 3)] >> (7 - (bitIdx & 7))) & 1;
                int lsb = (src[4 + ((bitIdx + 16) >> 3)] >> (7 - ((bitIdx + 16) & 7))) & 1;
                pixels[row * 4 + col] = paint[msb | (lsb << 1)];
            }
            return;
        }

        if (planar)
        {
            int ro = ((src[0] >> 1) & 0x3f);
            int go = ((src[0] & 1) << 6) | ((src[1] >> 1) & 0x3f) | (src[1] & 1);
            go = (go >> 1) & 0x7f;
            int bo = ((src[1] & 1) << 5) | ((src[2] >> 3) & 0x1f) | ((src[2] & 1) << 0);
            bo = ((src[1] & 1) << 5) | ((src[2] >> 1) & 0x18) | ((src[2] & 1) << 2) | ((src[3] >> 6) & 0x03);
            int rh = ((src[3] >> 1) & 0x1f) | ((src[3] & 1) << 5);
            int gh = ((src[4] >> 1) & 0x7f);
            int bh = ((src[4] & 1) << 5) | ((src[5] >> 3) & 0x1f);
            int rv = ((src[5] & 7) << 3) | ((src[6] >> 5) & 0x07);
            int gv = ((src[6] & 0x1f) << 2) | ((src[7] >> 6) & 0x03);
            int bv = src[7] & 0x3f;

            ro = (ro << 2) | (ro >> 4);
            go = (go << 1) | (go >> 6);
            bo = (bo << 2) | (bo >> 4);
            rh = (rh << 2) | (rh >> 4);
            gh = (gh << 1) | (gh >> 6);
            bh = (bh << 2) | (bh >> 4);
            rv = (rv << 2) | (rv >> 4);
            gv = (gv << 1) | (gv >> 6);
            bv = (bv << 2) | (bv >> 4);

            for (int y = 0; y < 4; y++)
            {
                for (int x = 0; x < 4; x++)
                {
                    int r = (x * (rh - ro) + y * (rv - ro) + 4 * ro + 2) >> 2;
                    int g = (x * (gh - go) + y * (gv - go) + 4 * go + 2) >> 2;
                    int b = (x * (bh - bo) + y * (bv - bo) + 4 * bo + 2) >> 2;
                    pixels[y * 4 + x] = { clampByte(r), clampByte(g), clampByte(b), 255 };
                }
            }
            return;
        }

        // individual/differential mode
        int table[2];
        table[0] = (block >> 37) & 7;
        table[1] = (block >> 34) & 7;

        for (int i = 0; i < 16; i++)
        {
            int col = i >> 2, row = i & 3;
            int sub = flipBit ? (row >= 2 ? 1 : 0) : (col >= 2 ? 1 : 0);
            int bitIdx = col * 4 + row;
            int msb = (src[4 + (bitIdx >> 3)] >> (7 - (bitIdx & 7))) & 1;
            int lsb = (src[4 + ((bitIdx + 16) >> 3)] >> (7 - ((bitIdx + 16) & 7))) & 1;
            int idx = msb | (lsb << 1);

            int mod = 0;
            switch (idx)
            {
            case 0:
                mod = Etc2Modifier[table[sub]][0];
                break;
            case 1:
                mod = -Etc2Modifier[table[sub]][0];
                break;
            case 2:
                mod = Etc2Modifier[table[sub]][1];
                break;
            case 3:
                mod = -Etc2Modifier[table[sub]][1];
                break;
            }

            pixels[row * 4 + col] = {
                clampByte(baseR[sub] + mod),
                clampByte(baseG[sub] + mod),
                clampByte(baseB[sub] + mod),
                255
            };
        }
    }

} // namespace

namespace gpu_decode
{

    void decodeBC1(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height)
    {
        uint32_t blocksW = (width + 3) / 4;
        uint32_t blocksH = (height + 3) / 4;

        for (uint32_t by = 0; by < blocksH; by++)
        {
            for (uint32_t bx = 0; bx < blocksW; bx++)
            {
                Rgba pixels[16];
                decodeBC1Block(src, pixels);
                src += 8;

                for (int py = 0; py < 4; py++)
                {
                    for (int px = 0; px < 4; px++)
                    {
                        writePixel(dst, bx * 4 + px, by * 4 + py, width, height, pixels[py * 4 + px]);
                    }
                }
            }
        }
    }

    void decodeBC2(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height)
    {
        uint32_t blocksW = (width + 3) / 4;
        uint32_t blocksH = (height + 3) / 4;

        for (uint32_t by = 0; by < blocksH; by++)
        {
            for (uint32_t bx = 0; bx < blocksW; bx++)
            {
                // First 8 bytes: explicit 4-bit alpha for each pixel
                const uint8_t* alphaBlock = src;

                Rgba pixels[16];
                decodeBC1Block(src + 8, pixels);
                src += 16;

                for (int py = 0; py < 4; py++)
                {
                    for (int px = 0; px < 4; px++)
                    {
                        int i = py * 4 + px;
                        int byteIdx = i / 2;
                        int shift = (i & 1) * 4;
                        uint8_t a4 = (alphaBlock[byteIdx] >> shift) & 0x0f;
                        pixels[i].a = static_cast<uint8_t>(a4 | (a4 << 4));
                        writePixel(dst, bx * 4 + px, by * 4 + py, width, height, pixels[i]);
                    }
                }
            }
        }
    }

    void decodeBC3(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height)
    {
        uint32_t blocksW = (width + 3) / 4;
        uint32_t blocksH = (height + 3) / 4;

        for (uint32_t by = 0; by < blocksH; by++)
        {
            for (uint32_t bx = 0; bx < blocksW; bx++)
            {
                uint8_t alphas[16];
                decodeBC3Alpha(src, alphas);

                Rgba pixels[16];
                decodeBC1Block(src + 8, pixels);
                src += 16;

                for (int py = 0; py < 4; py++)
                {
                    for (int px = 0; px < 4; px++)
                    {
                        int i = py * 4 + px;
                        pixels[i].a = alphas[i];
                        writePixel(dst, bx * 4 + px, by * 4 + py, width, height, pixels[i]);
                    }
                }
            }
        }
    }

    void decodeBC4(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height)
    {
        uint32_t blocksW = (width + 3) / 4;
        uint32_t blocksH = (height + 3) / 4;

        for (uint32_t by = 0; by < blocksH; by++)
        {
            for (uint32_t bx = 0; bx < blocksW; bx++)
            {
                uint8_t values[16];
                decodeBC3Alpha(src, values);
                src += 8;

                for (int py = 0; py < 4; py++)
                {
                    for (int px = 0; px < 4; px++)
                    {
                        int i = py * 4 + px;
                        Rgba c = { values[i], 0, 0, 255 };
                        writePixel(dst, bx * 4 + px, by * 4 + py, width, height, c);
                    }
                }
            }
        }
    }

    void decodeBC5(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height)
    {
        uint32_t blocksW = (width + 3) / 4;
        uint32_t blocksH = (height + 3) / 4;

        for (uint32_t by = 0; by < blocksH; by++)
        {
            for (uint32_t bx = 0; bx < blocksW; bx++)
            {
                uint8_t red[16];
                decodeBC3Alpha(src, red);
                uint8_t green[16];
                decodeBC3Alpha(src + 8, green);
                src += 16;

                for (int py = 0; py < 4; py++)
                {
                    for (int px = 0; px < 4; px++)
                    {
                        int i = py * 4 + px;
                        Rgba c = { red[i], green[i], 0, 255 };
                        writePixel(dst, bx * 4 + px, by * 4 + py, width, height, c);
                    }
                }
            }
        }
    }

    void decodeBC7(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height)
    {
        uint32_t blocksW = (width + 3) / 4;
        uint32_t blocksH = (height + 3) / 4;

        for (uint32_t by = 0; by < blocksH; by++)
        {
            for (uint32_t bx = 0; bx < blocksW; bx++)
            {
                Rgba pixels[16];
                decodeBC7Block(src, pixels);
                src += 16;

                for (int py = 0; py < 4; py++)
                {
                    for (int px = 0; px < 4; px++)
                    {
                        writePixel(dst, bx * 4 + px, by * 4 + py, width, height, pixels[py * 4 + px]);
                    }
                }
            }
        }
    }

    void decodeETC2_RGB(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height)
    {
        uint32_t blocksW = (width + 3) / 4;
        uint32_t blocksH = (height + 3) / 4;

        for (uint32_t by = 0; by < blocksH; by++)
        {
            for (uint32_t bx = 0; bx < blocksW; bx++)
            {
                Rgba pixels[16];
                decodeETC2Block_RGB(src, pixels);
                src += 8;

                for (int py = 0; py < 4; py++)
                {
                    for (int px = 0; px < 4; px++)
                    {
                        writePixel(dst, bx * 4 + px, by * 4 + py, width, height, pixels[py * 4 + px]);
                    }
                }
            }
        }
    }

    void decodeETC2_RGBA(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height)
    {
        uint32_t blocksW = (width + 3) / 4;
        uint32_t blocksH = (height + 3) / 4;

        for (uint32_t by = 0; by < blocksH; by++)
        {
            for (uint32_t bx = 0; bx < blocksW; bx++)
            {
                // first 8 bytes: EAC alpha block
                uint8_t alphas[16];
                decodeEACAlpha(src, alphas);

                // next 8 bytes: ETC2 RGB block
                Rgba pixels[16];
                decodeETC2Block_RGB(src + 8, pixels);
                src += 16;

                for (int py = 0; py < 4; py++)
                {
                    for (int px = 0; px < 4; px++)
                    {
                        int i = py * 4 + px;
                        pixels[i].a = alphas[i];
                        writePixel(dst, bx * 4 + px, by * 4 + py, width, height, pixels[i]);
                    }
                }
            }
        }
    }

    void decodeETC2_RGBA1(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height)
    {
        uint32_t blocksW = (width + 3) / 4;
        uint32_t blocksH = (height + 3) / 4;

        for (uint32_t by = 0; by < blocksH; by++)
        {
            for (uint32_t bx = 0; bx < blocksW; bx++)
            {
                // ETC2 punchthrough alpha uses the same 8-byte block as ETC2 RGB
                // but the diff bit signals opaque vs punchthrough mode.
                // When c0 <= c1 in the individual/differential path, index 2 = transparent black.
                uint64_t block = 0;
                for (int i = 0; i < 8; i++)
                {
                    block = (block << 8) | src[i];
                }

                bool diffBit = (block >> 33) & 1;
                bool flipBit = (block >> 32) & 1;

                int baseR[2], baseG[2], baseB[2];

                bool tMode = false;
                bool hMode = false;
                bool planar = false;

                if (diffBit)
                {
                    int r = (block >> 59) & 0x1f;
                    int dr = (block >> 56) & 0x07;
                    if (dr >= 4)
                        dr -= 8;
                    int rr = r + dr;
                    if (rr < 0 || rr > 31)
                    {
                        tMode = true;
                    }

                    int g = (block >> 51) & 0x1f;
                    int dg = (block >> 48) & 0x07;
                    if (dg >= 4)
                        dg -= 8;
                    int gg = g + dg;
                    if (!tMode && (gg < 0 || gg > 31))
                    {
                        hMode = true;
                    }

                    int b = (block >> 43) & 0x1f;
                    int db = (block >> 40) & 0x07;
                    if (db >= 4)
                        db -= 8;
                    int bb = b + db;
                    if (!tMode && !hMode && (bb < 0 || bb > 31))
                    {
                        planar = true;
                    }

                    if (!tMode && !hMode && !planar)
                    {
                        baseR[0] = (r << 3) | (r >> 2);
                        baseG[0] = (g << 3) | (g >> 2);
                        baseB[0] = (b << 3) | (b >> 2);
                        baseR[1] = (rr << 3) | (rr >> 2);
                        baseG[1] = (gg << 3) | (gg >> 2);
                        baseB[1] = (bb << 3) | (bb >> 2);
                    }
                }
                else
                {
                    // Non-differential: always opaque in punchthrough (spec says diffbit=0 is invalid
                    // for punchthrough, but we handle it gracefully as opaque)
                    baseR[0] = ((block >> 60) & 0xf) * 17;
                    baseG[0] = ((block >> 52) & 0xf) * 17;
                    baseB[0] = ((block >> 44) & 0xf) * 17;
                    baseR[1] = ((block >> 56) & 0xf) * 17;
                    baseG[1] = ((block >> 48) & 0xf) * 17;
                    baseB[1] = ((block >> 40) & 0xf) * 17;
                }

                // For T/H/planar modes, delegate to the RGB decoder (always opaque alpha)
                if (tMode || hMode || planar)
                {
                    Rgba pixels[16];
                    decodeETC2Block_RGB(src, pixels);
                    for (int py = 0; py < 4; py++)
                    {
                        for (int px = 0; px < 4; px++)
                        {
                            writePixel(dst, bx * 4 + px, by * 4 + py, width, height, pixels[py * 4 + px]);
                        }
                    }
                    src += 8;
                    continue;
                }

                // Individual/differential mode with punchthrough
                int table[2];
                table[0] = (block >> 37) & 7;
                table[1] = (block >> 34) & 7;

                Rgba pixels[16];
                for (int i = 0; i < 16; i++)
                {
                    int col = i >> 2, row = i & 3;
                    int sub = flipBit
                        ? (row >= 2 ? 1 : 0)
                        : (col >= 2 ? 1 : 0);
                    int bitIdx = col * 4 + row;
                    int msb = (src[4 + (bitIdx >> 3)] >> (7 - (bitIdx & 7))) & 1;
                    int lsb = (src[4 + ((bitIdx + 16) >> 3)] >> (7 - ((bitIdx + 16) & 7))) & 1;
                    int idx = msb | (lsb << 1);

                    // In punchthrough mode, index 2 (msb=0, lsb=1) = transparent black
                    if (idx == 2)
                    {
                        pixels[row * 4 + col] = { 0, 0, 0, 0 };
                        continue;
                    }

                    int mod = 0;
                    switch (idx)
                    {
                    case 0:
                        mod = Etc2Modifier[table[sub]][0];
                        break;
                    case 1:
                        mod = -Etc2Modifier[table[sub]][0];
                        break;
                    case 3:
                        mod = -Etc2Modifier[table[sub]][1];
                        break;
                    }

                    pixels[row * 4 + col] = {
                        clampByte(baseR[sub] + mod),
                        clampByte(baseG[sub] + mod),
                        clampByte(baseB[sub] + mod),
                        255
                    };
                }

                for (int py = 0; py < 4; py++)
                {
                    for (int px = 0; px < 4; px++)
                    {
                        writePixel(dst, bx * 4 + px, by * 4 + py, width, height, pixels[py * 4 + px]);
                    }
                }

                src += 8;
            }
        }
    }

    void decodeEAC_R11(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height)
    {
        uint32_t blocksW = (width + 3) / 4;
        uint32_t blocksH = (height + 3) / 4;

        for (uint32_t by = 0; by < blocksH; by++)
        {
            for (uint32_t bx = 0; bx < blocksW; bx++)
            {
                uint8_t values[16];
                decodeEACAlpha(src, values);
                src += 8;

                for (int py = 0; py < 4; py++)
                {
                    for (int px = 0; px < 4; px++)
                    {
                        int i = py * 4 + px;
                        Rgba c = { values[i], values[i], values[i], 255 };
                        writePixel(dst, bx * 4 + px, by * 4 + py, width, height, c);
                    }
                }
            }
        }
    }

    void decodeEAC_RG11(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height)
    {
        uint32_t blocksW = (width + 3) / 4;
        uint32_t blocksH = (height + 3) / 4;

        for (uint32_t by = 0; by < blocksH; by++)
        {
            for (uint32_t bx = 0; bx < blocksW; bx++)
            {
                uint8_t red[16];
                decodeEACAlpha(src, red);
                uint8_t green[16];
                decodeEACAlpha(src + 8, green);
                src += 16;

                for (int py = 0; py < 4; py++)
                {
                    for (int px = 0; px < 4; px++)
                    {
                        int i = py * 4 + px;
                        Rgba c = { red[i], green[i], 0, 255 };
                        writePixel(dst, bx * 4 + px, by * 4 + py, width, height, c);
                    }
                }
            }
        }
    }

    void decodeASTC(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height, uint32_t blockW, uint32_t blockH)
    {
        astcenc_config config{};
        astcenc_error status = astcenc_config_init(
            ASTCENC_PRF_LDR, blockW, blockH, 1,
            ASTCENC_PRE_FASTEST, ASTCENC_FLG_DECOMPRESS_ONLY, &config);
        if (status != ASTCENC_SUCCESS)
        {
            ::memset(dst, 0, width * height * 4);
            return;
        }

        astcenc_context* context = nullptr;
        status = astcenc_context_alloc(&config, 1, &context);
        if (status != ASTCENC_SUCCESS)
        {
            ::memset(dst, 0, width * height * 4);
            return;
        }

        astcenc_image image{};
        image.dim_x = width;
        image.dim_y = height;
        image.dim_z = 1;
        image.data_type = ASTCENC_TYPE_U8;
        image.data = reinterpret_cast<void**>(&dst);

        uint32_t blocksW_ = (width + blockW - 1) / blockW;
        uint32_t blocksH_ = (height + blockH - 1) / blockH;
        size_t dataLen = static_cast<size_t>(blocksW_) * blocksH_ * 16;

        astcenc_swizzle swizzle{ ASTCENC_SWZ_R, ASTCENC_SWZ_G, ASTCENC_SWZ_B, ASTCENC_SWZ_A };
        astcenc_decompress_image(context, src, dataLen, &image, &swizzle, 0);

        astcenc_context_free(context);
    }

} // namespace gpu_decode

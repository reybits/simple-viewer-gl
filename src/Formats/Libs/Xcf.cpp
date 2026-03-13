/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
#include "Log/Log.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <vector>
#include <zlib.h>

namespace
{
    // --- Big-endian file reading ---

    template <typename T>
    T ReadBE(cFile& file)
    {
        T val{};
        file.read(reinterpret_cast<char*>(&val), sizeof(T));
        auto p = reinterpret_cast<uint8_t*>(&val);
        std::reverse(p, p + sizeof(T));
        return val;
    }

    std::string ReadString(cFile& file)
    {
        auto len = ReadBE<uint32_t>(file);
        if (len == 0)
        {
            return {};
        }

        std::string s(len, '\0');
        file.read(s.data(), len);
        // Strip null terminator if present
        if (s.empty() == false && s.back() == '\0')
        {
            s.pop_back();
        }
        return s;
    }

    // --- XCF types ---

    enum class ColorMode : uint32_t
    {
        RGB       = 0,
        Grayscale = 1,
        Indexed   = 2,
    };

    enum class LayerType : uint32_t
    {
        RGB      = 0,
        RGBA     = 1,
        Gray     = 2,
        GrayA    = 3,
        Indexed  = 4,
        IndexedA = 5,
    };

    enum class Compression : uint8_t
    {
        None = 0,
        RLE  = 1,
        Zlib = 2,
    };

    enum class PropType : uint32_t
    {
        End          = 0,
        ColorMap     = 1,
        Opacity      = 6,
        Mode         = 7,
        Visible      = 8,
        Offset       = 15,
        Compression  = 17,
        FloatOpacity = 33,
    };

    struct Color
    {
        uint8_t r, g, b;
    };
    using Palette = std::array<Color, 256>;

    constexpr uint32_t TileSize     = 64;
    constexpr uint32_t MaxCanvasDim = 262144;

    // Read file offset: 32-bit for v0-v10, 64-bit for v11+
    uint64_t ReadOffset(cFile& file, bool wide)
    {
        if (wide)
        {
            return ReadBE<uint64_t>(file);
        }
        return ReadBE<uint32_t>(file);
    }

    // --- Property reading ---

    struct Property
    {
        PropType type;
        uint32_t length;
    };

    Property ReadProperty(cFile& file)
    {
        Property p;
        p.type   = ReadBE<PropType>(file);
        p.length = ReadBE<uint32_t>(file);
        return p;
    }

    // --- Tile decompression ---

    // Uncompressed: interleaved pixel data
    bool ReadTileNone(cFile& file, uint8_t* dst, uint32_t tileBytes)
    {
        return file.read(reinterpret_cast<char*>(dst), tileBytes) == tileBytes;
    }

    // RLE: per-channel planar encoding
    bool ReadTileRLE(cFile& file, uint8_t* dst, uint32_t tileW, uint32_t tileH, uint32_t bpp, uint32_t dataLen)
    {
        auto tilePixels = tileW * tileH;

        std::vector<uint8_t> compressed(dataLen);
        auto bytesRead = file.read(reinterpret_cast<char*>(compressed.data()), dataLen);
        if (bytesRead == 0)
        {
            return false;
        }

        auto src    = compressed.data();
        auto srcEnd = src + bytesRead;

        // Decode per-channel (planar), then de-planarize
        std::vector<uint8_t> planar(tilePixels * bpp);

        for (uint32_t ch = 0; ch < bpp; ch++)
        {
            auto chDst     = planar.data() + ch * tilePixels;
            auto remaining = tilePixels;

            while (remaining > 0)
            {
                if (src >= srcEnd)
                {
                    return false;
                }

                auto cmd = *src++;

                if (cmd >= 128)
                {
                    // Non-repeating run
                    uint32_t count = 256 - cmd;
                    if (count == 128)
                    {
                        if (src + 2 > srcEnd)
                        {
                            return false;
                        }
                        count = (static_cast<uint32_t>(src[0]) << 8) | src[1];
                        src += 2;
                    }

                    if (count > remaining || src + count > srcEnd)
                    {
                        return false;
                    }

                    std::memcpy(chDst, src, count);
                    src += count;
                    chDst += count;
                    remaining -= count;
                }
                else
                {
                    // Repeating run
                    uint32_t count = cmd + 1;
                    if (count == 128)
                    {
                        if (src + 2 > srcEnd)
                        {
                            return false;
                        }
                        count = (static_cast<uint32_t>(src[0]) << 8) | src[1];
                        src += 2;
                    }

                    if (count > remaining || src >= srcEnd)
                    {
                        return false;
                    }

                    auto val = *src++;
                    std::memset(chDst, val, count);
                    chDst += count;
                    remaining -= count;
                }
            }
        }

        // De-planarize: channel-separate → interleaved
        for (uint32_t i = 0; i < tilePixels; i++)
        {
            for (uint32_t ch = 0; ch < bpp; ch++)
            {
                dst[i * bpp + ch] = planar[ch * tilePixels + i];
            }
        }

        return true;
    }

    // Zlib: compressed interleaved pixel data
    bool ReadTileZlib(cFile& file, uint8_t* dst, uint32_t tileBytes, uint32_t dataLen)
    {
        std::vector<uint8_t> compressed(dataLen);
        auto bytesRead = file.read(reinterpret_cast<char*>(compressed.data()), dataLen);
        if (bytesRead == 0)
        {
            return false;
        }

        auto outSize = static_cast<uLongf>(tileBytes);
        auto res     = uncompress(dst, &outSize, compressed.data(),
                                  static_cast<uLong>(bytesRead));
        return res == Z_OK;
    }

    // Convert layer pixels to RGBA ---
    void ConvertToRGBA(const uint8_t* src, uint32_t pixelCount,
                       LayerType type, const Palette& palette, uint8_t* dst)
    {
        for (uint32_t i = 0; i < pixelCount; i++)
        {
            uint8_t r = 0, g = 0, b = 0, a = 255;

            switch (type)
            {
            case LayerType::RGB:
                r = src[i * 3 + 0];
                g = src[i * 3 + 1];
                b = src[i * 3 + 2];
                a = 255;
                break;

            case LayerType::RGBA:
                r = src[i * 4 + 0];
                g = src[i * 4 + 1];
                b = src[i * 4 + 2];
                a = src[i * 4 + 3];
                break;

            case LayerType::Gray:
                r = g = b = src[i];
                a         = 255;
                break;

            case LayerType::GrayA:
                r = g = b = src[i * 2 + 0];
                a         = src[i * 2 + 1];
                break;

            case LayerType::Indexed: {
                auto& c = palette[src[i]];
                r       = c.r;
                g       = c.g;
                b       = c.b;
                a       = 255;
                break;
            }

            case LayerType::IndexedA: {
                auto& c = palette[src[i * 2 + 0]];
                r       = c.r;
                g       = c.g;
                b       = c.b;
                a       = src[i * 2 + 1];
                break;
            }
            }

            dst[i * 4 + 0] = r;
            dst[i * 4 + 1] = g;
            dst[i * 4 + 2] = b;
            dst[i * 4 + 3] = a;
        }
    }

    // Alpha compositing (Porter-Duff "over") ---
    void CompositeLayer(const uint8_t* src, uint32_t srcW, uint32_t srcH,
                        int32_t offX, int32_t offY, uint8_t opacity,
                        uint8_t* dst, uint32_t dstW, uint32_t dstH)
    {
        for (uint32_t sy = 0; sy < srcH; sy++)
        {
            auto dy = static_cast<int32_t>(sy) + offY;
            if (dy < 0 || dy >= static_cast<int32_t>(dstH))
            {
                continue;
            }

            for (uint32_t sx = 0; sx < srcW; sx++)
            {
                auto dx = static_cast<int32_t>(sx) + offX;
                if (dx < 0 || dx >= static_cast<int32_t>(dstW))
                {
                    continue;
                }

                const auto sp = &src[(sy * srcW + sx) * 4];
                auto dp       = &dst[(static_cast<uint32_t>(dy) * dstW
                                      + static_cast<uint32_t>(dx))
                                     * 4];

                // Effective source alpha with layer opacity
                auto sa = static_cast<uint32_t>(sp[3]) * opacity / 255;
                if (sa == 0)
                {
                    continue;
                }

                auto da = static_cast<uint32_t>(dp[3]);

                // outA = sa + da * (1 - sa/255)
                auto outA = sa + da * (255 - sa) / 255;
                if (outA == 0)
                {
                    continue;
                }

                // outC = (srcC * sa + dstC * da * (1 - sa/255)) / outA
                auto daWeight = da * (255 - sa) / 255;
                for (int c = 0; c < 3; c++)
                {
                    dp[c] = static_cast<uint8_t>((static_cast<uint32_t>(sp[c]) * sa
                                                  + static_cast<uint32_t>(dp[c]) * daWeight)
                                                 / outA);
                }
                dp[3] = static_cast<uint8_t>(outA);
            }
        }
    }

    // --- Layer parsing ---

    struct LayerInfo
    {
        uint32_t width  = 0;
        uint32_t height = 0;
        LayerType type  = LayerType::RGBA;
        std::string name;
        int32_t offsetX       = 0;
        int32_t offsetY       = 0;
        uint8_t opacity       = 255;
        bool visible          = true;
        uint64_t hierarchyPtr = 0;
    };

    LayerInfo ReadLayerHeader(cFile& file, bool wideOffsets)
    {
        LayerInfo layer;
        layer.width  = ReadBE<uint32_t>(file);
        layer.height = ReadBE<uint32_t>(file);
        layer.type   = ReadBE<LayerType>(file);
        layer.name   = ReadString(file);

        // Read layer properties
        while (true)
        {
            auto prop = ReadProperty(file);
            if (prop.type == PropType::End)
            {
                break;
            }

            auto propStart = file.getOffset();

            switch (prop.type)
            {
            case PropType::Opacity:
                layer.opacity = static_cast<uint8_t>(std::min(ReadBE<uint32_t>(file), 255u));
                break;

            case PropType::FloatOpacity: {
                auto f        = ReadBE<float>(file);
                layer.opacity = static_cast<uint8_t>(std::clamp(f, 0.0f, 1.0f) * 255.0f + 0.5f);
                break;
            }

            case PropType::Visible:
                layer.visible = ReadBE<uint32_t>(file) != 0;
                break;

            case PropType::Offset:
                layer.offsetX = ReadBE<int32_t>(file);
                layer.offsetY = ReadBE<int32_t>(file);
                break;

            default:
                break;
            }

            // Seek past remaining property data
            file.seek(propStart + prop.length, SEEK_SET);
        }

        layer.hierarchyPtr = ReadOffset(file, wideOffsets);
        ReadOffset(file, wideOffsets); // mask pointer (unused)

        return layer;
    }

    // Decode a single layer's tile data and convert to RGBA
    std::vector<uint8_t> DecodeLayer(cFile& file, const LayerInfo& layer,
                                     Compression compression, const Palette& palette,
                                     bool wideOffsets)
    {
        file.seek(static_cast<size_t>(layer.hierarchyPtr), SEEK_SET);

        auto hierW    = ReadBE<uint32_t>(file);
        auto hierH    = ReadBE<uint32_t>(file);
        auto hierBpp  = ReadBE<uint32_t>(file);
        auto levelPtr = ReadOffset(file, wideOffsets);

        // Read first (full-resolution) level only
        file.seek(static_cast<size_t>(levelPtr), SEEK_SET);
        auto levelW = ReadBE<uint32_t>(file);
        auto levelH = ReadBE<uint32_t>(file);

        // Read tile pointers
        std::vector<uint64_t> tilePtrs;
        while (true)
        {
            auto ptr = ReadOffset(file, wideOffsets);
            if (ptr == 0)
            {
                break;
            }
            tilePtrs.push_back(ptr);
        }

        auto tileCountX = (levelW + TileSize - 1) / TileSize;

        // Decode all tiles into a raw pixel buffer
        std::vector<uint8_t> rawBuffer(hierW * hierH * hierBpp, 0);

        for (size_t i = 0; i < tilePtrs.size(); i++)
        {
            auto tileCol = static_cast<uint32_t>(i) % tileCountX;
            auto tileRow = static_cast<uint32_t>(i) / tileCountX;

            auto tileX     = tileCol * TileSize;
            auto tileY     = tileRow * TileSize;
            auto tileW     = std::min(TileSize, levelW - tileX);
            auto tileH     = std::min(TileSize, levelH - tileY);
            auto tileBytes = tileW * tileH * hierBpp;

            // Data length from consecutive tile pointers (or file end for last tile)
            uint32_t dataLen;
            if (i + 1 < tilePtrs.size())
            {
                dataLen = static_cast<uint32_t>(tilePtrs[i + 1] - tilePtrs[i]);
            }
            else
            {
                dataLen = static_cast<uint32_t>(file.getSize() - tilePtrs[i]);
            }

            file.seek(static_cast<size_t>(tilePtrs[i]), SEEK_SET);

            std::vector<uint8_t> tileBuf(tileBytes);
            bool ok = false;

            switch (compression)
            {
            case Compression::None:
                ok = ReadTileNone(file, tileBuf.data(), tileBytes);
                break;
            case Compression::RLE:
                ok = ReadTileRLE(file, tileBuf.data(), tileW, tileH, hierBpp, dataLen);
                break;
            case Compression::Zlib:
                ok = ReadTileZlib(file, tileBuf.data(), tileBytes, dataLen);
                break;
            }

            if (ok == false)
            {
                cLog::Warning("XCF: failed to decode tile {}.", i);
                continue;
            }

            // Copy tile into layer buffer
            for (uint32_t row = 0; row < tileH; row++)
            {
                auto srcOff = row * tileW * hierBpp;
                auto dstOff = ((tileY + row) * hierW + tileX) * hierBpp;
                std::memcpy(&rawBuffer[dstOff], &tileBuf[srcOff], tileW * hierBpp);
            }
        }

        // Convert to RGBA
        auto pixelCount = hierW * hierH;
        std::vector<uint8_t> rgba(pixelCount * 4);
        ConvertToRGBA(rawBuffer.data(), pixelCount, layer.type, palette, rgba.data());

        return rgba;
    }

} // namespace

namespace xcf
{
    bool import(cFile& file, sChunkData& chunk, sImageInfo& info)
    {
        file.seek(0, SEEK_SET);

        // Header: "gimp xcf " (9 bytes) + version tag (4 bytes) + null (1 byte)
        char sig[9];
        file.read(sig, 9);
        if (std::memcmp(sig, "gimp xcf ", 9) != 0)
        {
            cLog::Error("XCF: invalid signature.");
            return false;
        }

        char verTag[5];
        file.read(verTag, 5);

        uint32_t version = 0;
        if (verTag[0] == 'v')
        {
            version = static_cast<uint32_t>(std::atoi(verTag + 1));
        }

        const bool wideOffsets = (version >= 11);

        auto width  = ReadBE<uint32_t>(file);
        auto height = ReadBE<uint32_t>(file);
        ReadBE<ColorMode>(file); // color mode (used implicitly via layer types)

        if (width == 0 || height == 0 || width > MaxCanvasDim || height > MaxCanvasDim)
        {
            cLog::Error("XCF: invalid dimensions {}x{}.", width, height);
            return false;
        }

        // v4-v10 store a precision field; v345+ (GIMP 3.0) removed it from the header.
        if (version >= 4 && version < 11)
        {
            auto precision = ReadBE<uint32_t>(file);

            bool is8bit = false;
            if (version < 7)
            {
                // v4-v6: 0=u8-gamma, 100=u8-linear
                is8bit = (precision == 0 || precision == 100);
            }
            else
            {
                // v7-v10: 100=u8-linear, 150=u8-gamma
                is8bit = (precision == 100 || precision == 150);
            }

            if (is8bit == false)
            {
                cLog::Error("XCF: only 8-bit channels supported (precision={}).", precision);
                return false;
            }
        }

        // Read global properties
        Compression compression = Compression::None;
        Palette palette{};

        while (static_cast<uint64_t>(file.getOffset()) + 8 <= static_cast<uint64_t>(file.getSize()))
        {
            auto prop = ReadProperty(file);
            if (prop.type == PropType::End)
            {
                break;
            }

            auto propStart = file.getOffset();

            if (static_cast<uint64_t>(propStart) + prop.length > static_cast<uint64_t>(file.getSize()))
            {
                cLog::Warning("XCF: file truncated, skipping remaining properties.");
                break;
            }

            switch (prop.type)
            {
            case PropType::Compression:
                compression = static_cast<Compression>(ReadBE<uint8_t>(file));
                break;

            case PropType::ColorMap: {
                auto count = ReadBE<uint32_t>(file);
                for (uint32_t i = 0; i < count && i < 256; i++)
                {
                    file.read(reinterpret_cast<char*>(&palette[i]), 3);
                }
                break;
            }

            default:
                break;
            }

            file.seek(propStart + prop.length, SEEK_SET);
        }

        // Read layer pointers
        const auto offsetSize = wideOffsets
            ? 8u
            : 4u;
        const auto fileSize   = static_cast<uint64_t>(file.getSize());
        std::vector<uint64_t> layerPtrs;
        while (static_cast<uint64_t>(file.getOffset()) + offsetSize <= fileSize)
        {
            auto ptr = ReadOffset(file, wideOffsets);
            if (ptr == 0 || ptr >= fileSize)
            {
                break;
            }
            layerPtrs.push_back(ptr);
        }

        if (layerPtrs.empty())
        {
            cLog::Error("XCF: no layers found (file may be truncated).");
            return false;
        }

        // Read and decode visible layers
        struct DecodedLayer
        {
            std::vector<uint8_t> rgba;
            uint32_t width;
            uint32_t height;
            int32_t offsetX;
            int32_t offsetY;
            uint8_t opacity;
        };

        std::vector<DecodedLayer> layers;

        for (auto ptr : layerPtrs)
        {
            file.seek(static_cast<size_t>(ptr), SEEK_SET);
            auto layerInfo = ReadLayerHeader(file, wideOffsets);

            if (layerInfo.visible == false)
            {
                continue;
            }

            auto rgba = DecodeLayer(file, layerInfo, compression, palette, wideOffsets);

            layers.push_back({
                std::move(rgba),
                layerInfo.width,
                layerInfo.height,
                layerInfo.offsetX,
                layerInfo.offsetY,
                layerInfo.opacity,
            });
        }

        // Allocate output canvas
        info.images   = 1;
        info.bppImage = 32;
        chunk.width   = width;
        chunk.height  = height;
        chunk.allocate(width, height, 32, ePixelFormat::RGBA);
        std::fill(chunk.bitmap.begin(), chunk.bitmap.end(), 0);

        // Composite layers bottom-to-top (first in list = top, last = bottom)
        for (auto it = layers.rbegin(); it != layers.rend(); ++it)
        {
            CompositeLayer(
                it->rgba.data(), it->width, it->height,
                it->offsetX, it->offsetY, it->opacity,
                chunk.bitmap.data(), width, height);
        }

        return true;
    }

} // namespace xcf

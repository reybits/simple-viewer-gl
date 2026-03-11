/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatIco.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
#include "Libs/PngReader.h"
#include "Log/Log.h"

#include <cstring>

#pragma pack(push, 1)
struct IcoHeader
{
    uint16_t reserved; // Reserved. Should always be 0.
    uint16_t type;     // Specifies image type: 1 for icon (.ICO) image, 2 for cursor (.CUR) image. Other values are invalid.
    uint16_t count;    // Specifies number of images in the file.
};

// List of icons.
struct IcoDirentry
{
    uint8_t width;    // Specifies image width in pixels. 0 means 256.
    uint8_t height;   // Specifies image height in pixels. 0 means 256.
    uint8_t colors;   // Number of colors in palette. 0 if truecolor.
    uint8_t reserved; // Reserved. Should be 0.
    uint16_t planes;  // ICO: color planes (0 or 1). CUR: hotspot X.
    uint16_t bits;    // ICO: bits per pixel. CUR: hotspot Y.
    uint32_t size;    // Size of bitmap data in bytes.
    uint32_t offset;  // Offset of bitmap data in the file.
};

// Variant of BMP InfoHeader (40 bytes).
struct IcoBmpInfoHeader
{
    uint32_t size;
    uint32_t width;
    uint32_t height; // XOR-Bitmap height + AND-Bitmap height
    uint16_t planes;
    uint16_t bits;
    uint32_t reserved0; // Compression = 0
    uint32_t imagesize;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
    uint32_t reserved4;
};
#pragma pack(pop)

namespace
{
    // Standard DWORD-aligned row pitch.
    uint32_t DwordAlignedPitch(uint32_t width, uint32_t bpp)
    {
        return ((width * bpp + 31) / 32) * 4;
    }

    uint32_t GetBit(const uint8_t* data, uint32_t bit, uint32_t width)
    {
        const auto w = DwordAlignedPitch(width, 1);
        const auto line = bit / width;
        const auto offset = bit % width;
        return (data[line * w + offset / 8] >> (7 - (offset % 8))) & 1;
    }

    uint32_t GetNibble(const uint8_t* data, uint32_t nibble, uint32_t width)
    {
        const auto w = DwordAlignedPitch(width, 4);
        const auto line = nibble / width;
        const auto offset = nibble % width;
        const auto byte = data[line * w + offset / 2];
        return (offset % 2 == 0)
            ? (byte >> 4)
            : (byte & 0x0F);
    }

    uint32_t GetByte(const uint8_t* data, uint32_t byte, uint32_t width)
    {
        const auto w = DwordAlignedPitch(width, 8);
        const auto line = byte / width;
        const auto offset = byte % width;
        return data[line * w + offset];
    }

    using IndexFn = uint32_t (*)(const uint8_t*, uint32_t, uint32_t);

    void DecodePaletted(uint8_t* out, const sChunkData& chunk,
                        const uint32_t* palette, const uint8_t* xorMask, const uint8_t* andMask,
                        IndexFn getIndex)
    {
        for (uint32_t y = 0; y < chunk.height; y++)
        {
            auto idx = (chunk.height - y - 1) * chunk.pitch;
            for (uint32_t x = 0; x < chunk.width; x++)
            {
                const auto pos = y * chunk.width + x;
                const auto color = palette[getIndex(xorMask, pos, chunk.width)];
                ::memcpy(&out[idx], &color, 3);
                out[idx + 3] = GetBit(andMask, pos, chunk.width)
                    ? 0
                    : 255;
                idx += 4;
            }
        }
    }

} // namespace

bool cFormatIco::isSupported(cFile& file, Buffer& buffer) const
{
    if (readBuffer(file, buffer, sizeof(IcoHeader)) == false)
    {
        return false;
    }

    const auto h = reinterpret_cast<const IcoHeader*>(buffer.data());
    return h->reserved == 0
        && h->count > 0
        && h->count * sizeof(IcoDirentry) <= static_cast<size_t>(file.getSize())
        && (h->type == 1 || h->type == 2);
}

bool cFormatIco::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    m_filename = filename;
    return load(0, chunk, info);
}

bool cFormatIco::LoadSubImageImpl(uint32_t current, sChunkData& chunk, sImageInfo& info)
{
    return load(current, chunk, info);
}

bool cFormatIco::load(uint32_t current, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (openFile(file, m_filename.c_str(), info) == false)
    {
        return false;
    }

    IcoHeader header;
    if (sizeof(header) != file.read(&header, sizeof(header)))
    {
        return false;
    }

    std::vector<char> buffer(header.count * sizeof(IcoDirentry));
    auto images = reinterpret_cast<const IcoDirentry*>(buffer.data());
    if (buffer.size() != file.read(buffer.data(), buffer.size()))
    {
        return false;
    }

    current = std::min(current, static_cast<uint32_t>(header.count - 1));

    const auto image = &images[current];
    cLog::Debug("-- IcoDirentry");
    cLog::Debug("  Width  : {}", image->width);
    cLog::Debug("  Height : {}", image->height);
    cLog::Debug("  Colors : {}", image->colors);
    cLog::Debug("  Planes : {}", image->planes);
    cLog::Debug("  Bits   : {}", image->bits);
    cLog::Debug("  Size   : {}", image->size);
    cLog::Debug("  Offset : {}", image->offset);

    bool result = false;

    if (image->colors == 0 && image->width == 0 && image->height == 0)
    {
        info.formatName = "ico/png";
        result = loadPngFrame(chunk, info, file, image);
    }
    else
    {
        info.formatName = "ico";
        result = loadOrdinaryFrame(chunk, info, file, image);
    }

    info.images = header.count;
    info.current = current;

    return result;
}

bool cFormatIco::loadPngFrame(sChunkData& chunk, sImageInfo& info, cFile& file, const IcoDirentry* image)
{
    auto size = image->size;
    std::vector<uint8_t> buffer(size);
    auto data = buffer.data();

    file.seek(image->offset, SEEK_SET);
    if (size != file.read(data, size))
    {
        cLog::Error("Can't read ICO/PNG frame.");
        return false;
    }

    cPngReader reader;
    reader.setProgressCallback([this](float percent) {
        updateProgress(percent);
    });

    // ICC is applied per-scanline inside loadPng()
    auto result = reader.loadPng(chunk, info, data, size);
    if (result && reader.getIccProfile().empty() == false)
    {
        info.formatName = "ico/png/icc";
    }

    return result;
}

bool cFormatIco::loadOrdinaryFrame(sChunkData& chunk, sImageInfo& info, cFile& file, const IcoDirentry* image)
{
    file.seek(image->offset, SEEK_SET);
    std::vector<uint8_t> p(image->size);
    if (image->size != file.read(p.data(), p.size()))
    {
        cLog::Error("Can't read icon data.");
        return false;
    }

    auto imgHeader = reinterpret_cast<const IcoBmpInfoHeader*>(p.data());
    chunk.width = imgHeader->width;
    chunk.height = imgHeader->height / 2; // xor mask + and mask
    info.bppImage = imgHeader->bits;

    if (info.bppImage != 1 && info.bppImage != 4 && info.bppImage != 8
        && info.bppImage != 24 && info.bppImage != 32)
    {
        cLog::Error("Unsupported ICO bit depth: {}.", info.bppImage);
        return false;
    }

    chunk.allocate(chunk.width, chunk.height, 32, ePixelFormat::BGRA);

    const auto pitch = DwordAlignedPitch(chunk.width, info.bppImage);
    auto out = chunk.bitmap.data();

    cLog::Debug("-- IcoBmpInfoHeader");
    cLog::Debug("  Size       : {}", imgHeader->size);
    cLog::Debug("  Width      : {}", imgHeader->width);
    cLog::Debug("  Height     : {}", imgHeader->height);
    cLog::Debug("  Planes     : {}", imgHeader->planes);
    cLog::Debug("  Bits       : {}", imgHeader->bits);
    cLog::Debug("  Image size : {}", imgHeader->imagesize);

    uint32_t colors = image->colors;
    if (info.bppImage < 16)
    {
        colors = colors == 0
            ? (1u << info.bppImage)
            : image->colors;
    }
    const auto palette = reinterpret_cast<const uint32_t*>(p.data() + imgHeader->size);
    const auto xorMask = p.data() + imgHeader->size + colors * 4;
    const auto andMask = xorMask + chunk.height * pitch;

    if (info.bppImage <= 8)
    {
        static constexpr IndexFn Lookup[] = { nullptr, GetBit, nullptr, nullptr, GetNibble, nullptr, nullptr, nullptr, GetByte };
        DecodePaletted(out, chunk, palette, xorMask, andMask, Lookup[info.bppImage]);
    }
    else
    {
        const uint32_t bpp = info.bppImage / 8;
        for (uint32_t y = 0; y < chunk.height; y++)
        {
            const auto* row = xorMask + pitch * y;
            auto idx = (chunk.height - y - 1) * chunk.pitch;
            for (uint32_t x = 0; x < chunk.width; x++)
            {
                ::memcpy(&out[idx], row, 3);
                out[idx + 3] = (info.bppImage < 32)
                    ? (GetBit(andMask, y * chunk.width + x, chunk.width)
                           ? 0
                           : 255)
                    : row[3];

                idx += 4;
                row += bpp;
            }
        }
    }

    return true;
}

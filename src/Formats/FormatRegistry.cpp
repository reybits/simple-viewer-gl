/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatRegistry.h"
#include "Common/File.h"
#include "Common/Helpers.h"
#include "Format.h"

#include "FormatAge.h"
#include "FormatBmp.h"
#include "FormatDds.h"
#include "FormatEps.h"
#include "FormatExr.h"
#include "FormatGif.h"
#include "FormatIcns.h"
#include "FormatIco.h"
#include "FormatJp2k.h"
#include "FormatJpeg.h"
#include "FormatPng.h"
#include "FormatPnm.h"
#include "FormatPsd.h"
#include "FormatPvr.h"
#include "FormatRaw.h"
#include "FormatScr.h"
#include "FormatSvg.h"
#include "FormatTarga.h"
#include "FormatTiff.h"
#include "FormatWebP.h"
#include "FormatXcf.h"
#include "FormatXpm.h"
#include "FormatXwd.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace
{
    // Magic bytes for simple detection
    constexpr uint8_t MagicJpeg[][4] = {
        { 0xff, 0xd8, 0xff, 0xdb },
        { 0xff, 0xd8, 0xff, 0xe0 },
        { 0xff, 0xd8, 0xff, 0xed },
        { 0xff, 0xd8, 0xff, 0xe1 },
        { 0xff, 0xd8, 0xff, 0xe2 },
        { 0xff, 0xd8, 0xff, 0xee },
        { 0xff, 0xd8, 0xff, 0xfe },
    };

    constexpr uint8_t MagicGif87a[] = { 'G', 'I', 'F', '8', '7', 'a' };
    constexpr uint8_t MagicGif89a[] = { 'G', 'I', 'F', '8', '9', 'a' };
    constexpr uint8_t MagicTiffLE[] = { 0x49, 0x49, 0x2A, 0x00 };
    constexpr uint8_t MagicTiffBE[] = { 0x4D, 0x4D, 0x00, 0x2A };
#if defined(OPENEXR_SUPPORT)
    constexpr uint8_t MagicExr[] = { 0x76, 0x2f, 0x31, 0x01 };
#endif
    constexpr uint8_t MagicXcf[] = { 'g', 'i', 'm', 'p', ' ', 'x', 'c', 'f' };
    constexpr uint8_t MagicJp2[] = { 0x00, 0x00, 0x00, 0x0C, 0x6A, 0x50, 0x20, 0x20, 0x0D, 0x0A, 0x87, 0x0A };
    constexpr uint8_t MagicXpm[] = { '/', '*', ' ', 'X', 'P', 'M', ' ', '*', '/' };

    // --- Probe functions for formats with complex detection ---

    bool probeJpeg(cFile& /*file*/, Buffer& /*buffer*/, const uint8_t* data, uint32_t dataSize, uint64_t /*fileSize*/)
    {
        if (dataSize < 4)
        {
            return false;
        }
        for (const auto& magic : MagicJpeg)
        {
            if (::memcmp(data, magic, 4) == 0)
            {
                return true;
            }
        }
        return false;
    }

    bool probeGif(cFile& /*file*/, Buffer& /*buffer*/, const uint8_t* data, uint32_t dataSize, uint64_t /*fileSize*/)
    {
        if (dataSize < 6)
        {
            return false;
        }
        return ::memcmp(data, MagicGif87a, 6) == 0
            || ::memcmp(data, MagicGif89a, 6) == 0;
    }

    bool probeTiff(cFile& /*file*/, Buffer& /*buffer*/, const uint8_t* data, uint32_t dataSize, uint64_t /*fileSize*/)
    {
        if (dataSize < 4)
        {
            return false;
        }
        return ::memcmp(data, MagicTiffLE, 4) == 0
            || ::memcmp(data, MagicTiffBE, 4) == 0;
    }

    bool probePnm(cFile& /*file*/, Buffer& /*buffer*/, const uint8_t* data, uint32_t dataSize, uint64_t fileSize)
    {
        if (dataSize < 2)
        {
            return false;
        }
        return data[0] == 'P' && data[1] >= '1' && data[1] <= '6' && fileSize >= 8;
    }

    bool probeIco(cFile& /*file*/, Buffer& /*buffer*/, const uint8_t* data, uint32_t dataSize, uint64_t fileSize)
    {
        if (dataSize < 6)
        {
            return false;
        }
#pragma pack(push, 1)
        struct IcoHeader
        {
            uint16_t reserved;
            uint16_t type;
            uint16_t count;
        };
#pragma pack(pop)
        auto h = reinterpret_cast<const IcoHeader*>(data);
        return h->reserved == 0 && h->count > 0
            && h->count * 16u <= fileSize
            && (h->type == 1 || h->type == 2);
    }

    bool probeTga(cFile& /*file*/, Buffer& /*buffer*/, const uint8_t* data, uint32_t dataSize, uint64_t /*fileSize*/)
    {
        if (dataSize < 18)
        {
            return false;
        }
#pragma pack(push, 1)
        struct TgaHeader
        {
            uint8_t idLength;
            uint8_t colorMapType;
            uint8_t imageType;
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
        auto h = reinterpret_cast<const TgaHeader*>(data);
        return h->colorMapType <= 1
            && h->width > 0 && h->height > 0
            && (h->imageType == 1 || h->imageType == 2 || h->imageType == 3
                || h->imageType == 9 || h->imageType == 10 || h->imageType == 11)
            && (h->pixelDepth == 8 || h->pixelDepth == 16 || h->pixelDepth == 24 || h->pixelDepth == 32);
    }

    bool probeIcns(cFile& /*file*/, Buffer& /*buffer*/, const uint8_t* data, uint32_t dataSize, uint64_t fileSize)
    {
        if (dataSize < 8)
        {
            return false;
        }
        const uint8_t magic[] = { 'i', 'c', 'n', 's' };
        if (::memcmp(data, magic, 4) != 0)
        {
            return false;
        }
        uint32_t fileLen = helpers::read_uint32(data + 4);
        return fileLen == fileSize;
    }

    bool probeWebp(cFile& /*file*/, Buffer& /*buffer*/, const uint8_t* data, uint32_t dataSize, uint64_t fileSize)
    {
        if (dataSize < 12)
        {
            return false;
        }
        const uint8_t riff[] = { 'R', 'I', 'F', 'F' };
        const uint8_t webp[] = { 'W', 'E', 'B', 'P' };
        uint32_t size;
        ::memcpy(&size, data + 4, 4);
        return size == fileSize - 8
            && ::memcmp(data, riff, 4) == 0
            && ::memcmp(data + 8, webp, 4) == 0;
    }

    bool probeBmp(cFile& /*file*/, Buffer& /*buffer*/, const uint8_t* data, uint32_t dataSize, uint64_t fileSize)
    {
        if (dataSize < 14)
        {
            return false;
        }
#pragma pack(push, 1)
        struct BmpHeader
        {
            uint8_t id[2];
            uint32_t fileSize;
            uint16_t reserved1;
            uint16_t reserved2;
            uint32_t bitmapOffset;
        };
#pragma pack(pop)
        auto h = reinterpret_cast<const BmpHeader*>(data);
        const uint8_t ids[][2] = {
            { 'B', 'M' }, { 'B', 'A' }, { 'C', 'I' },
            { 'C', 'P' }, { 'I', 'C' }, { 'P', 'T' },
        };
        for (const auto& id : ids)
        {
            if (h->id[0] == id[0] && h->id[1] == id[1]
                && h->fileSize == fileSize
                && h->bitmapOffset < fileSize)
            {
                return true;
            }
        }
        return false;
    }

    bool probePsd(cFile& /*file*/, Buffer& /*buffer*/, const uint8_t* data, uint32_t dataSize, uint64_t /*fileSize*/)
    {
        if (dataSize < 26)
        {
            return false;
        }
        return ::memcmp(data, "8BPS", 4) == 0;
    }

    bool probeDds(cFile& /*file*/, Buffer& /*buffer*/, const uint8_t* data, uint32_t dataSize, uint64_t /*fileSize*/)
    {
        if (dataSize < 128)
        {
            return false;
        }
        return ::memcmp(data, "DDS ", 4) == 0;
    }

    bool probeXwd(cFile& /*file*/, Buffer& /*buffer*/, const uint8_t* data, uint32_t dataSize, uint64_t fileSize)
    {
        if (dataSize < 8)
        {
            return false;
        }
        // Big-endian header
        uint32_t headerSize = helpers::read_uint32(data);
        uint32_t fileVersion = helpers::read_uint32(data + 4);
        return ((headerSize == 40 && fileVersion == 0x06)   // X10
                || (headerSize == 100 && fileVersion == 0x07)) // X11
            && headerSize < fileSize;
    }

    // Formats that delegate to the reader's own isSupported (complex multi-step probes)
    bool probeSvg(cFile& file, Buffer& buffer, const uint8_t* /*data*/, uint32_t /*dataSize*/, uint64_t /*fileSize*/)
    {
        cFormatSvg reader(nullptr);
        return reader.isSupported(file, buffer);
    }

    bool probeEps(cFile& file, Buffer& buffer, const uint8_t* /*data*/, uint32_t /*dataSize*/, uint64_t /*fileSize*/)
    {
        cFormatEps reader(nullptr);
        return reader.isSupported(file, buffer);
    }

    bool probePvr(cFile& file, Buffer& buffer, const uint8_t* /*data*/, uint32_t /*dataSize*/, uint64_t /*fileSize*/)
    {
        cFormatPvr reader(nullptr);
        return reader.isSupported(file, buffer);
    }

    bool probeScr(cFile& file, Buffer& buffer, const uint8_t* /*data*/, uint32_t /*dataSize*/, uint64_t /*fileSize*/)
    {
        cFormatScr reader(nullptr);
        return reader.isSupported(file, buffer);
    }

    bool probeAge(cFile& file, Buffer& buffer, const uint8_t* /*data*/, uint32_t /*dataSize*/, uint64_t /*fileSize*/)
    {
        cFormatAge reader(nullptr);
        return reader.isSupported(file, buffer);
    }

    bool probeRaw(cFile& file, Buffer& buffer, const uint8_t* /*data*/, uint32_t /*dataSize*/, uint64_t /*fileSize*/)
    {
        cFormatRaw reader(nullptr);
        return reader.isSupported(file, buffer);
    }

    // --- Factory functions ---

    template <typename T>
    std::unique_ptr<cFormat> makeFormat(sCallbacks* cb)
    {
        return std::make_unique<T>(cb);
    }

} // namespace

const std::vector<sFormatEntry>& FormatRegistry::getRegistry()
{
    static const std::vector<sFormatEntry> registry = {
        // Detection priority order: common formats first, fallback last
        { "jpeg", nullptr, 0, 0, probeJpeg, makeFormat<cFormatJpeg>, 4 },
        { "png", nullptr, 0, 0, [](cFile& file, Buffer& buffer, const uint8_t* /*data*/, uint32_t /*dataSize*/, uint64_t /*fileSize*/) -> bool {
            cFormatPng reader(nullptr);
            return reader.isSupported(file, buffer);
        }, makeFormat<cFormatPng>, 8 },
        { "bmp", nullptr, 0, 0, probeBmp, makeFormat<cFormatBmp>, 14 },
#if defined(GIF_SUPPORT)
        { "gif", nullptr, 0, 0, probeGif, makeFormat<cFormatGif>, 6 },
#endif
        { "psd", nullptr, 0, 0, probePsd, makeFormat<cFormatPsd>, 26 },
        { "xcf", MagicXcf, sizeof(MagicXcf), 0, nullptr, makeFormat<cFormatXcf>, 8 },
        { "ico", nullptr, 0, 0, probeIco, makeFormat<cFormatIco>, 6 },
        { "tga", nullptr, 0, 0, probeTga, makeFormat<cFormatTarga>, 18 },
#if defined(TIFF_SUPPORT)
        { "tiff", nullptr, 0, 0, probeTiff, makeFormat<cFormatTiff>, 4 },
#endif
        { "eps", nullptr, 0, 0, probeEps, makeFormat<cFormatEps>, 256 },
        { "dds", nullptr, 0, 0, probeDds, makeFormat<cFormatDds>, 128 },
        { "xwd", nullptr, 0, 0, probeXwd, makeFormat<cFormatXwd>, 8 },
        { "xpm", MagicXpm, sizeof(MagicXpm), 0, nullptr, makeFormat<cFormatXpm>, 9 },
        { "pnm", nullptr, 0, 0, probePnm, makeFormat<cFormatPnm>, 2 },
        { "icns", nullptr, 0, 0, probeIcns, makeFormat<cFormatIcns>, 8 },
#if defined(WEBP_SUPPORT)
        { "webp", nullptr, 0, 0, probeWebp, makeFormat<cFormatWebP>, 12 },
#endif
#if defined(JPEG2000_SUPPORT)
        { "jp2k", MagicJp2, sizeof(MagicJp2), 0, nullptr, makeFormat<cFormatJp2k>, 12 },
#endif
        { "age", nullptr, 0, 0, probeAge, makeFormat<cFormatAge>, 28 },
        { "svg", nullptr, 0, 0, probeSvg, makeFormat<cFormatSvg>, 256 },
        { "raw", nullptr, 0, 0, probeRaw, makeFormat<cFormatRaw>, 20 },
        { "pvr", nullptr, 0, 0, probePvr, makeFormat<cFormatPvr>, 64 },
        { "scr", nullptr, 0, 0, probeScr, makeFormat<cFormatScr>, 4 },
#if defined(OPENEXR_SUPPORT)
        { "exr", MagicExr, sizeof(MagicExr), 0, nullptr, makeFormat<cFormatExr>, 4 },
#endif
    };

    return registry;
}

const sFormatEntry* FormatRegistry::detect(cFile& file, Buffer& buffer)
{
    auto& registry = getRegistry();

    for (auto& entry : registry)
    {
        file.seek(0, SEEK_SET);

        if (entry.magic != nullptr)
        {
            // Simple magic-based detection
            uint32_t needed = entry.magicOffset + entry.magicSize;
            if (buffer.size() < needed)
            {
                file.seek(0, SEEK_SET);
                buffer.resize(needed);
                if (file.read(buffer.data(), needed) != needed)
                {
                    continue;
                }
            }
            if (::memcmp(buffer.data() + entry.magicOffset, entry.magic, entry.magicSize) == 0)
            {
                return &entry;
            }
        }
        else if (entry.probe != nullptr)
        {
            // Ensure minimum buffer for probe
            if (buffer.size() < entry.minProbeSize && entry.minProbeSize > 0)
            {
                uint32_t toRead = std::min<uint32_t>(static_cast<uint32_t>(file.getSize()), entry.minProbeSize);
                file.seek(0, SEEK_SET);
                buffer.resize(toRead);
                if (file.read(buffer.data(), toRead) != toRead)
                {
                    continue;
                }
            }
            file.seek(0, SEEK_SET);
            if (entry.probe(file, buffer, buffer.data(), static_cast<uint32_t>(buffer.size()), file.getSize()))
            {
                return &entry;
            }
        }
    }

    return nullptr;
}

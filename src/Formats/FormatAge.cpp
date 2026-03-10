/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatAge.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
#include "Common/ZlibDecoder.h"
#include "Libs/AGEheader.h"
#include "Libs/GpuDecode.h"
#include "Libs/Rle.h"
#include "Log/Log.h"

#include <cstring>
#include <lz4/lz4hc.h>

namespace
{
    bool isValidFormat(const AGE::Header& header, unsigned file_size)
    {
        if (header.data_size + sizeof(AGE::Header) != file_size)
        {
            return false;
        }

        return AGE::isValidHeader(header);
    }

    struct CompressedFormatInfo
    {
        unsigned blockW;
        unsigned blockH;
        unsigned blockBytes;
    };

    const CompressedFormatInfo* getCompressedFormatInfo(AGE::Format format)
    {
        static const CompressedFormatInfo Formats[] = {
            { 4, 4, 16 }, // ASTC_4x4
            { 6, 6, 16 }, // ASTC_6x6
            { 8, 8, 16 }, // ASTC_8x8
            { 4, 4, 8 },  // ETC2_RGB
            { 4, 4, 16 }, // ETC2_RGBA
            { 4, 4, 8 },  // BC1
            { 4, 4, 16 }, // BC3
            { 4, 4, 16 }, // BC7
        };

        if (!AGE::isCompressedFormat(format))
        {
            return nullptr;
        }

        auto idx = static_cast<unsigned>(format) - static_cast<unsigned>(AGE::Format::LAST_UNCOMPRESSED) - 1;
        return &Formats[idx];
    }

    bool decodeCompressedToRGBA(AGE::Format format, const uint8_t* src, uint8_t* dst, unsigned w, unsigned h)
    {
        switch (format)
        {
        case AGE::Format::BC1:
            gpu_decode::decodeBC1(src, dst, w, h);
            return true;
        case AGE::Format::BC3:
            gpu_decode::decodeBC3(src, dst, w, h);
            return true;
        case AGE::Format::BC7:
            gpu_decode::decodeBC7(src, dst, w, h);
            return true;
        case AGE::Format::ETC2_RGB:
            gpu_decode::decodeETC2_RGB(src, dst, w, h);
            return true;
        case AGE::Format::ETC2_RGBA:
            gpu_decode::decodeETC2_RGBA(src, dst, w, h);
            return true;
        case AGE::Format::ASTC_4x4:
            gpu_decode::decodeASTC(src, dst, w, h, 4, 4);
            return true;
        case AGE::Format::ASTC_6x6:
            gpu_decode::decodeASTC(src, dst, w, h, 6, 6);
            return true;
        case AGE::Format::ASTC_8x8:
            gpu_decode::decodeASTC(src, dst, w, h, 8, 8);
            return true;
        default:
            return false;
        }
    }

} // namespace

bool cFormatAge::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, sizeof(AGE::Header)))
    {
        return false;
    }

    auto header = reinterpret_cast<const AGE::Header*>(buffer.data());
    return isValidFormat(*header, file.getSize());
}

bool cFormatAge::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (!openFile(file, filename, info))
    {
        return false;
    }

    AGE::Header header;
    if (sizeof(header) != file.read(&header, sizeof(header)))
    {
        cLog::Error("Invalid AGE image format.");
        return false;
    }

    if (!isValidFormat(header, info.fileSize))
    {
        return false;
    }

    auto version = AGE::getVersion(header);
    auto format = header.format;

    if (version == 1)
    {
        format = AGE::remapV1Format(static_cast<unsigned>(header.format));
        if (format == AGE::Format::UNKNOWN)
        {
            cLog::Error("Unknown AGE v1 format.");
            return false;
        }
    }

    const bool isCompressed = AGE::isCompressedFormat(format);
    unsigned compressedDataSize = 0;

    if (isCompressed)
    {
        auto fmtInfo = getCompressedFormatInfo(format);
        auto blocksW = (header.w + fmtInfo->blockW - 1) / fmtInfo->blockW;
        auto blocksH = (header.h + fmtInfo->blockH - 1) / fmtInfo->blockH;
        compressedDataSize = blocksW * blocksH * fmtInfo->blockBytes;
    }

    unsigned bytespp = 0;
    switch (format)
    {
    case AGE::Format::RGBA8888:
        bytespp = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case AGE::Format::RGBA5551:
        bytespp = 2;
        chunk.format = ePixelFormat::RGBA5551;
        break;
    case AGE::Format::RGBA4444:
        bytespp = 2;
        chunk.format = ePixelFormat::RGBA4444;
        break;
    case AGE::Format::RGB888:
        bytespp = 3;
        chunk.format = ePixelFormat::RGB;
        break;
    case AGE::Format::RGB565:
        bytespp = 2;
        chunk.format = ePixelFormat::RGB565;
        break;
    case AGE::Format::A8:
        bytespp = 1;
        chunk.format = ePixelFormat::Alpha;
        break;
    default:
        if (!isCompressed)
        {
            cLog::Error("Unknown AGE format.");
            return false;
        }
        // compressed formats will be decoded to RGBA
        bytespp = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    }

    chunk.width = header.w;
    chunk.height = header.h;

    // For compressed: first decompress AGE compression into a temp buffer,
    // then software-decode GPU blocks to RGBA.
    if (isCompressed)
    {
        std::vector<uint8_t> compressedBuf(compressedDataSize);

        if (header.compression != AGE::Compression::NONE)
        {
            std::vector<unsigned char> in(header.data_size);
            if (header.data_size != file.read(in.data(), header.data_size))
            {
                return false;
            }

            updateProgress(0.3f);

            unsigned decoded = 0;
            if (header.compression == AGE::Compression::LZ4 || header.compression == AGE::Compression::LZ4HC)
            {
                decoded = LZ4_decompress_safe(reinterpret_cast<const char*>(in.data()), reinterpret_cast<char*>(compressedBuf.data()), in.size(), compressedBuf.size());
            }
            else if (header.compression == AGE::Compression::ZLIB)
            {
                cZlibDecoder decoder;
                decoded = decoder.decode(in.data(), in.size(), compressedBuf.data(), compressedBuf.size());
            }
            else
            {
                cRLE decoder;
                if (header.compression == AGE::Compression::RLE4)
                {
                    decoded = decoder.decodeBy4(reinterpret_cast<unsigned*>(in.data()), in.size() / 4, reinterpret_cast<unsigned*>(compressedBuf.data()), compressedBuf.size() / 4);
                }
                else
                {
                    decoded = decoder.decode(in.data(), in.size(), compressedBuf.data(), compressedBuf.size());
                }
            }

            if (decoded == 0)
            {
                cLog::Error("Can't decompress AGE data.");
                return false;
            }
        }
        else
        {
            if (compressedDataSize != file.read(compressedBuf.data(), compressedDataSize))
            {
                return false;
            }
        }

        updateProgress(0.6f);

        info.bppImage = bytespp * 8;
        chunk.allocate(chunk.width, chunk.height, bytespp * 8, chunk.format);

        if (!decodeCompressedToRGBA(format, compressedBuf.data(), chunk.bitmap.data(), chunk.width, chunk.height))
        {
            cLog::Error("Failed to decode compressed AGE texture.");
            return false;
        }

        updateProgress(1.0f);
    }
    else
    {
        info.bppImage = bytespp * 8;
        chunk.allocate(chunk.width, chunk.height, bytespp * 8, chunk.format);
        unsigned dataSize = chunk.pitch * chunk.height;

        if (header.compression != AGE::Compression::NONE)
        {
            std::vector<unsigned char> in(header.data_size);
            if (header.data_size != file.read(in.data(), header.data_size))
            {
                return false;
            }

            updateProgress(0.5f);

            unsigned decoded = 0;

            if (header.compression == AGE::Compression::LZ4 || header.compression == AGE::Compression::LZ4HC)
            {
                decoded = LZ4_decompress_safe(reinterpret_cast<const char*>(in.data()), reinterpret_cast<char*>(chunk.bitmap.data()), in.size(), chunk.bitmap.size());
                if (decoded <= 0)
                {
                    cLog::Error("Can't decode {}.",
                               header.compression == AGE::Compression::LZ4
                                   ? "LZ4"
                                   : "LZ4HC");
                    return false;
                }
            }
            else if (header.compression == AGE::Compression::ZLIB)
            {
                cZlibDecoder decoder;
                decoded = decoder.decode(in.data(), in.size(), chunk.bitmap.data(), chunk.bitmap.size());
                if (decoded == 0)
                {
                    cLog::Error("Can't decode ZLIB data.");
                    return false;
                }
            }
            else
            {
                cRLE decoder;
                if (header.compression == AGE::Compression::RLE4)
                {
                    decoded = decoder.decodeBy4(reinterpret_cast<unsigned*>(in.data()), in.size() / 4, reinterpret_cast<unsigned*>(chunk.bitmap.data()), chunk.bitmap.size() / 4);
                }
                else
                {
                    decoded = decoder.decode(in.data(), in.size(), chunk.bitmap.data(), chunk.bitmap.size());
                }

                if (decoded == 0)
                {
                    cLog::Error("Can't decode RLE data.");
                    return false;
                }
            }

            updateProgress(1.0f);
        }
        else
        {
            if (dataSize != file.read(chunk.bitmap.data(), dataSize))
            {
                return false;
            }
            updateProgress(1.0f);
        }
    }

    info.formatName = AGE::FormatToStr(format);

    return true;
}

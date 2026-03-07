/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "formatage.h"
#include "common/ZlibDecoder.h"
#include "common/bitmap_description.h"
#include "common/file.h"
#include "libs/AGEheader.h"
#include "libs/gpu_decode.h"
#include "libs/rle.h"

#include <cstdio>
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
        static const CompressedFormatInfo Formats[] =
        {
            { 4, 4, 16 },  // ASTC_4x4
            { 6, 6, 16 },  // ASTC_6x6
            { 8, 8, 16 },  // ASTC_8x8
            { 4, 4,  8 },  // ETC2_RGB
            { 4, 4, 16 },  // ETC2_RGBA
            { 4, 4,  8 },  // BC1
            { 4, 4, 16 },  // BC3
            { 4, 4, 16 },  // BC7
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

cFormatAge::cFormatAge(iCallbacks* callbacks)
    : cFormat(callbacks)
{
}

cFormatAge::~cFormatAge()
{
}

bool cFormatAge::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, sizeof(AGE::Header)))
    {
        return false;
    }

    auto header = reinterpret_cast<const AGE::Header*>(buffer.data());
    return isValidFormat(*header, file.getSize());
}

bool cFormatAge::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    cFile file;
    if (!file.open(filename))
    {
        return false;
    }

    desc.size = file.getSize();

    AGE::Header header;
    if (sizeof(header) != file.read(&header, sizeof(header)))
    {
        ::printf("(EE) Not valid AGE image format.\n");
        return false;
    }

    if (!isValidFormat(header, desc.size))
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
            ::printf("(EE) Unknown AGE v1 format.\n");
            return false;
        }
    }

    const bool isCompressed = AGE::isCompressedFormat(format);
    unsigned compressedDataSize = 0;

    if (isCompressed)
    {
        auto info = getCompressedFormatInfo(format);
        auto blocksW = (header.w + info->blockW - 1) / info->blockW;
        auto blocksH = (header.h + info->blockH - 1) / info->blockH;
        compressedDataSize = blocksW * blocksH * info->blockBytes;
    }

    unsigned bytespp = 0;
    switch (format)
    {
    case AGE::Format::RGBA8888:
        bytespp = 4;
        desc.format = GL_RGBA;
        break;
    case AGE::Format::RGBA5551:
        bytespp = 2;
        desc.format = GL_UNSIGNED_SHORT_5_5_5_1;
        break;
    case AGE::Format::RGBA4444:
        bytespp = 2;
        desc.format = GL_UNSIGNED_SHORT_4_4_4_4;
        break;
    case AGE::Format::RGB888:
        bytespp = 3;
        desc.format = GL_RGB;
        break;
    case AGE::Format::RGB565:
        bytespp = 2;
        desc.format = GL_UNSIGNED_SHORT_5_6_5;
        break;
    case AGE::Format::A8:
        bytespp = 1;
        desc.format = GL_ALPHA;
        break;
    default:
        if (!isCompressed)
        {
            ::printf("(EE) Unknown AGE format.\n");
            return false;
        }
        // compressed formats will be decoded to RGBA
        bytespp = 4;
        desc.format = GL_RGBA;
        break;
    }

    desc.width = header.w;
    desc.height = header.h;

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
                ::printf("(EE) Error decompressing AGE data.\n");
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

        desc.bppImage = bytespp * 8;
        desc.bpp = bytespp * 8;
        desc.pitch = desc.width * bytespp;
        desc.bitmap.resize(desc.pitch * desc.height);

        if (!decodeCompressedToRGBA(format, compressedBuf.data(), desc.bitmap.data(), desc.width, desc.height))
        {
            ::printf("(EE) Failed to decode compressed AGE texture.\n");
            return false;
        }

        updateProgress(1.0f);
    }
    else
    {
        desc.bpp = desc.bppImage = bytespp * 8;
        desc.pitch = desc.width * bytespp;
        unsigned dataSize = desc.pitch * desc.height;
        desc.bitmap.resize(dataSize);

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
                decoded = LZ4_decompress_safe(reinterpret_cast<const char*>(in.data()), reinterpret_cast<char*>(desc.bitmap.data()), in.size(), desc.bitmap.size());
                if (decoded <= 0)
                {
                    ::printf("(EE) Error decode %s.\n",
                             header.compression == AGE::Compression::LZ4
                                 ? "LZ4"
                                 : "LZ4HC");
                    return false;
                }
            }
            else if (header.compression == AGE::Compression::ZLIB)
            {
                cZlibDecoder decoder;
                decoded = decoder.decode(in.data(), in.size(), desc.bitmap.data(), desc.bitmap.size());
                if (decoded == 0)
                {
                    ::printf("(EE) Error decode ZLIB.\n");
                    return false;
                }
            }
            else
            {
                cRLE decoder;
                if (header.compression == AGE::Compression::RLE4)
                {
                    decoded = decoder.decodeBy4(reinterpret_cast<unsigned*>(in.data()), in.size() / 4, reinterpret_cast<unsigned*>(desc.bitmap.data()), desc.bitmap.size() / 4);
                }
                else
                {
                    decoded = decoder.decode(in.data(), in.size(), desc.bitmap.data(), desc.bitmap.size());
                }

                if (decoded == 0)
                {
                    ::printf("(EE) Error decode RLE.\n");
                    return false;
                }
            }

            updateProgress(1.0f);
        }
        else
        {
            if (dataSize != file.read(desc.bitmap.data(), dataSize))
            {
                return false;
            }
            updateProgress(1.0f);
        }
    }

    m_formatName = AGE::FormatToStr(format);

    return true;
}

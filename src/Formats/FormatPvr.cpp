/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatPvr.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/Helpers.h"
#include "Common/ImageInfo.h"
#include "Libs/GpuDecode.h"
#include "Log/Log.h"

#include <cstring>
#include <pvrtc/PVRTDecompress.h>
#include <zlib.h>

namespace
{
    struct CCZHeader
    {
        uint8_t sig[4]; // Signature. Should be 'CCZ!' 4 bytes.

        enum CompressionType : uint16_t
        {
            ZLIB,  // zlib format.
            BZIP2, // bzip2 format (not supported yet).
            GZIP,  // gzip format (not supported yet).
            NONE,  // plain (not supported yet).
        };

        CompressionType compressionType;

        uint16_t version;  // Should be 2 (although version type==1 is also supported).
        uint32_t reserved; // Reserved for users.
        uint32_t len;      // Size of the uncompressed file.
    };

    constexpr uint32_t PVR_TEXTURE_FLAG_TYPE_MASK = 0xff;

    enum class PVR2TextureFlag : uint32_t
    {
        Mipmap = 1 << 8,        // has mip map levels
        Twiddle = 1 << 9,       // is twiddled
        Bumpmap = 1 << 10,      // has normals encoded for a bump map
        Tiling = 1 << 11,       // is bordered for tiled pvr
        Cubemap = 1 << 12,      // is a cubemap/skybox
        FalseMipCol = 1 << 13,  // are there false colored MIP levels
        Volume = 1 << 14,       // is this a volume texture
        Alpha = 1 << 15,        // v2.1 is there transparency info in the texture
        VerticalFlip = 1 << 16, // v2.1 is the texture vertically flipped
    };

    enum class PVR2TexturePixelFormat : uint32_t
    {
        RGBA4444 = 0x10,
        RGBA5551,
        RGBA8888,
        RGB565,
        RGB555,
        RGB888,
        I8,
        AI88,
        PVRTC2BPP_RGBA,
        PVRTC4BPP_RGBA,
        BGRA8888,
        A8,
    };

    struct PVRv2TexHeader
    {
        uint32_t headerLength;
        uint32_t height;
        uint32_t width;
        uint32_t numMipmaps;
        uint32_t flags;
        uint32_t dataLength;
        uint32_t bpp;
        uint32_t bitmaskRed;
        uint32_t bitmaskGreen;
        uint32_t bitmaskBlue;
        uint32_t bitmaskAlpha;
        uint32_t pvrTag;
        uint32_t numSurfs;
    };

    bool isPvr2(const uint8_t* buffer, uint32_t size)
    {
        if (size < sizeof(PVRv2TexHeader))
        {
            return false;
        }

        auto header = reinterpret_cast<const PVRv2TexHeader*>(buffer);
        return ::memcmp(&header->pvrTag, "PVR!", 4) == 0;
    }

    // -------------------------------------------------------------------------

    enum class PVR3TexturePixelFormat : uint64_t
    {
        PVRTC2BPP_RGB = 0ULL,
        PVRTC2BPP_RGBA = 1ULL,
        PVRTC4BPP_RGB = 2ULL,
        PVRTC4BPP_RGBA = 3ULL,
        PVRTC2_2BPP_RGBA = 4ULL,
        PVRTC2_4BPP_RGBA = 5ULL,
        ETC1 = 6ULL,
        DXT1 = 7ULL,
        DXT2 = 8ULL,
        DXT3 = 9ULL,
        DXT4 = 10ULL,
        DXT5 = 11ULL,
        BC1 = 7ULL,
        BC2 = 9ULL,
        BC3 = 11ULL,
        BC4 = 12ULL,
        BC5 = 13ULL,
        BC6 = 14ULL,
        BC7 = 15ULL,
        UYVY = 16ULL,
        YUY2 = 17ULL,
        BW1bpp = 18ULL,
        R9G9B9E5 = 19ULL,
        RGBG8888 = 20ULL,
        GRGB8888 = 21ULL,
        ETC2_RGB = 22ULL,
        ETC2_RGBA = 23ULL,
        ETC2_RGBA1 = 24ULL,
        EAC_R11_Unsigned = 25ULL,
        EAC_R11_Signed = 26ULL,
        EAC_RG11_Unsigned = 27ULL,
        EAC_RG11_Signed = 28ULL,

        BGRA8888 = 0x0808080861726762ULL,
        RGBA8888 = 0x0808080861626772ULL,
        RGBA4444 = 0x0404040461626772ULL,
        RGBA5551 = 0x0105050561626772ULL,
        RGB565 = 0x0005060500626772ULL,
        RGB888 = 0x0008080800626772ULL,
        A8 = 0x0000000800000061ULL,
        L8 = 0x000000080000006cULL,
        LA88 = 0x000008080000616cULL,
    };

    struct PVRv3TexHeader
    {
        uint32_t version;
        uint32_t flags;
        uint64_t pixelFormat;
        uint32_t colorSpace;
        uint32_t channelType;
        uint32_t height;
        uint32_t width;
        uint32_t depth;
        uint32_t numberOfSurfaces;
        uint32_t numberOfFaces;
        uint32_t numberOfMipmaps;
        uint32_t metadataLength;
    };

    bool isPvr3(const uint8_t* buffer, uint32_t size)
    {
        if (size < sizeof(PVRv3TexHeader))
        {
            return false;
        }

        auto version = helpers::read_uint32(buffer);
        return version == 0x50565203;
    }

    void convertRgb555toRgb888(uint8_t* dst, const uint8_t* src, uint32_t pixelCount)
    {
        auto in = reinterpret_cast<const uint16_t*>(src);
        for (uint32_t i = 0; i < pixelCount; i++)
        {
            const auto pixel = in[i];
            const auto r5 = static_cast<uint8_t>((pixel >> 10) & 0x1F);
            const auto g5 = static_cast<uint8_t>((pixel >> 5) & 0x1F);
            const auto b5 = static_cast<uint8_t>(pixel & 0x1F);
            dst[i * 3 + 0] = static_cast<uint8_t>((r5 << 3) | (r5 >> 2));
            dst[i * 3 + 1] = static_cast<uint8_t>((g5 << 3) | (g5 >> 2));
            dst[i * 3 + 2] = static_cast<uint8_t>((b5 << 3) | (b5 >> 2));
        }
    }

    bool inflateCCZBuffer(const uint8_t* buffer, uint32_t bufferLen, Buffer& bitmap)
    {
        auto& header = *reinterpret_cast<const CCZHeader*>(buffer);

        if (::memcmp(&header, "CCZ", 3) != 0)
        {
            return false;
        }

        auto& s = header.sig;
        cLog::Debug("Type: '{}{}{}{}'.",
                    static_cast<char>(s[0]), static_cast<char>(s[1]),
                    static_cast<char>(s[2]), static_cast<char>(s[3]));

        if (header.sig[3] == '!')
        {
            auto version = helpers::read_uint16(reinterpret_cast<const uint8_t*>(&header.version));
            cLog::Debug("  Version          : {}", version);
            if (version > 2)
            {
                cLog::Error("Unsupported CCZ header format.");
                return false;
            }

            auto compressionType = helpers::read_uint16(reinterpret_cast<const uint8_t*>(&header.compressionType));
            cLog::Debug("  Compression type : {}", compressionType);
            if (compressionType != CCZHeader::CompressionType::ZLIB)
            {
                cLog::Error("Unsupported CCZ compression method.");
                return false;
            }
        }
        else if (header.sig[3] == 'p')
        {
            cLog::Error("Encrypted CCZ files are not supported.");
            return false;
        }
        else
        {
            cLog::Error("Invalid CCZ file.");
            return false;
        }

        auto size = helpers::read_uint32(reinterpret_cast<const uint8_t*>(&header.len));
        cLog::Debug("  Size             : {}", size);

        bitmap.resize(size);

        unsigned long destlen = size;
        auto source = buffer + sizeof(CCZHeader);
        auto sourceLen = bufferLen - sizeof(CCZHeader);
        auto ret = uncompress(bitmap.data(), &destlen, source, sourceLen);

        return ret == Z_OK;
    }

    bool inflateMemory(const uint8_t* in, uint32_t inLength, Buffer& bitmap)
    {
        size_t bufferSize = 256 * 1024;
        bitmap.resize(bufferSize);

        z_stream d_stream;
        d_stream.zalloc = nullptr;
        d_stream.zfree = nullptr;
        d_stream.opaque = nullptr;

        d_stream.next_in = const_cast<uint8_t*>(in);
        d_stream.avail_in = inLength;
        d_stream.next_out = bitmap.data();
        d_stream.avail_out = static_cast<uInt>(bufferSize);

        if (inflateInit2(&d_stream, 15 + 32) != Z_OK)
        {
            return false;
        }

        for (;;)
        {
            auto err = inflate(&d_stream, Z_NO_FLUSH);
            if (err == Z_STREAM_END)
            {
                break;
            }

            if (err == Z_NEED_DICT)
            {
                err = Z_DATA_ERROR;
            }
            if (err == Z_DATA_ERROR || err == Z_MEM_ERROR)
            {
                inflateEnd(&d_stream);
                return false;
            }

            if (err != Z_STREAM_END)
            {
                constexpr auto Factor = 2u;

                bitmap.resize(bufferSize * Factor);

                d_stream.next_out = bitmap.data() + bufferSize;
                d_stream.avail_out = static_cast<uInt>(bufferSize);
                bufferSize *= Factor;
            }
        }

        bitmap.resize(bufferSize - d_stream.avail_out);

        return inflateEnd(&d_stream) == Z_OK;
    }

    using GpuDecodeFunc = void (*)(const uint8_t*, uint8_t*, uint32_t, uint32_t);

} // namespace

bool cFormatPvr::isGZipBuffer(const uint8_t* buffer, uint32_t size) const
{
    return size > 2 && buffer[0] == 0x1F && buffer[1] == 0x8B;
}

bool cFormatPvr::isGZipBuffer(cFile& file, Buffer& buffer) const
{
    if (readBuffer(file, buffer, 2) == false)
    {
        return false;
    }

    return isGZipBuffer(buffer.data(), buffer.size());
}

bool cFormatPvr::isCCZBuffer(const uint8_t* buffer, uint32_t size) const
{
    if (size < sizeof(CCZHeader))
    {
        return false;
    }

    auto header = reinterpret_cast<const CCZHeader*>(buffer);
    if (::memcmp(header, "CCZ", 3) != 0)
    {
        return false;
    }

    return header->sig[3] == '!' || header->sig[3] == 'p';
}

bool cFormatPvr::isCCZBuffer(cFile& file, Buffer& buffer) const
{
    if (readBuffer(file, buffer, sizeof(CCZHeader)) == false)
    {
        return false;
    }

    return isCCZBuffer(buffer.data(), buffer.size());
}

bool cFormatPvr::isSupported(cFile& file, Buffer& buffer) const
{
    if (isCCZBuffer(file, buffer))
    {
        cLog::Debug("Detected CCZ buffer.");
        return true;
    }

    if (isGZipBuffer(file, buffer))
    {
        cLog::Debug("Detected GZip buffer.");
        return true;
    }

    return isPvr2(buffer.data(), buffer.size()) || isPvr3(buffer.data(), buffer.size());
}

bool cFormatPvr::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (!openFile(file, filename, info))
    {
        return false;
    }
    file.seek(0, SEEK_SET);

    Buffer buffer(info.fileSize);

    if (file.read(buffer.data(), static_cast<uint32_t>(buffer.size())) != static_cast<uint32_t>(buffer.size()))
    {
        cLog::Error("Can't read file.");
        return false;
    }

    Buffer unpacked;
    const uint8_t* unpackedData = nullptr;
    uint32_t unpackedSize = 0;

    if (isCCZBuffer(buffer.data(), buffer.size()))
    {
        if (inflateCCZBuffer(buffer.data(), buffer.size(), unpacked) == false)
        {
            cLog::Error("Can't decompress CCZ data.");
            return false;
        }

        unpackedData = unpacked.data();
        unpackedSize = unpacked.size();
    }
    else if (isGZipBuffer(buffer.data(), buffer.size()))
    {
        if (inflateMemory(buffer.data(), buffer.size(), unpacked) == false)
        {
            cLog::Error("Can't decompress GZip data.");
            return false;
        }

        unpackedData = unpacked.data();
        unpackedSize = unpacked.size();
    }
    else
    {
        unpackedData = buffer.data();
        unpackedSize = buffer.size();
    }

    if (isPvr2(unpackedData, unpackedSize))
    {
        return loadPvr2(unpackedData, chunk, info);
    }
    else if (isPvr3(unpackedData, unpackedSize))
    {
        return loadPvr3(unpackedData, chunk, info);
    }

    cLog::Error("Unknown PVR format.");

    return false;
}

bool cFormatPvr::loadPvr2(const uint8_t* data, sChunkData& chunk, sImageInfo& info)
{
    info.formatName = "pvr2";

    auto& header = *reinterpret_cast<const PVRv2TexHeader*>(data);

    auto flags = header.flags;
    auto pixelFormat = static_cast<PVR2TexturePixelFormat>(flags & PVR_TEXTURE_FLAG_TYPE_MASK);
    auto flipped = (flags & static_cast<uint32_t>(PVR2TextureFlag::VerticalFlip)) != 0;
    if (flipped)
    {
        cLog::Warning("Image is flipped. Regenerate it using PVRTexTool.");
    }

    auto width = header.width;
    auto height = header.height;

    cLog::Debug("-- PVR2 header");
    cLog::Debug("  Header length  : {}", header.headerLength);
    cLog::Debug("  Width          : {}", width);
    cLog::Debug("  Height         : {}", height);
    cLog::Debug("  Mipmaps        : {}", header.numMipmaps);
    cLog::Debug("  Flags          : {:#x}", header.flags);
    cLog::Debug("  Data length    : {}", header.dataLength);
    cLog::Debug("  BPP            : {}", header.bpp);
    cLog::Debug("  Mask R         : {:#010x}", header.bitmaskRed);
    cLog::Debug("  Mask G         : {:#010x}", header.bitmaskGreen);
    cLog::Debug("  Mask B         : {:#010x}", header.bitmaskBlue);
    cLog::Debug("  Mask A         : {:#010x}", header.bitmaskAlpha);
    cLog::Debug("  Surfaces       : {}", header.numSurfs);

    enum class Decomp
    {
        Copy,
        PVRTC2,
        PVRTC4,
        RGB555,
    };
    auto decompress = Decomp::Copy;
    auto bytes = 0u;
    switch (pixelFormat)
    {
    case PVR2TexturePixelFormat::RGBA4444:
        bytes = 2;
        chunk.format = ePixelFormat::RGBA4444;
        break;
    case PVR2TexturePixelFormat::RGBA5551:
        bytes = 2;
        chunk.format = ePixelFormat::RGBA5551;
        break;
    case PVR2TexturePixelFormat::RGBA8888:
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case PVR2TexturePixelFormat::RGB565:
        bytes = 2;
        chunk.format = ePixelFormat::RGB565;
        break;
    case PVR2TexturePixelFormat::RGB555:
        decompress = Decomp::RGB555;
        bytes = 3;
        chunk.format = ePixelFormat::RGB;
        break;
    case PVR2TexturePixelFormat::RGB888:
        bytes = 3;
        chunk.format = ePixelFormat::RGB;
        break;
    case PVR2TexturePixelFormat::BGRA8888:
        bytes = 4;
        chunk.format = ePixelFormat::BGRA;
        break;
    case PVR2TexturePixelFormat::A8:
        bytes = 1;
        chunk.format = ePixelFormat::Alpha;
        break;
    case PVR2TexturePixelFormat::I8:
        bytes = 1;
        chunk.format = ePixelFormat::Luminance;
        break;
    case PVR2TexturePixelFormat::AI88:
        bytes = 2;
        chunk.format = ePixelFormat::LuminanceAlpha;
        break;
    case PVR2TexturePixelFormat::PVRTC2BPP_RGBA:
        decompress = Decomp::PVRTC2;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case PVR2TexturePixelFormat::PVRTC4BPP_RGBA:
        decompress = Decomp::PVRTC4;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;

    default:
        cLog::Error("Unsupported PVR2 pixel format: {:#x}.", static_cast<uint32_t>(pixelFormat));
        return false;
    }

    chunk.bpp = bytes * 8;
    info.bppImage = bytes * 8;
    chunk.width = width;
    chunk.height = height;
    chunk.pitch = width * bytes;
    chunk.resizeBitmap(chunk.pitch, chunk.height);
    auto src = data + sizeof(PVRv2TexHeader);

    switch (decompress)
    {
    case Decomp::Copy:
        ::memcpy(chunk.bitmap.data(), src, chunk.bitmap.size());
        break;
    case Decomp::PVRTC2:
        if (pvr::PVRTDecompressPVRTC(src, 1, width, height, chunk.bitmap.data()) == 0)
        {
            cLog::Error("PVRTC2 decompression failed.");
            return false;
        }
        break;
    case Decomp::PVRTC4:
        if (pvr::PVRTDecompressPVRTC(src, 0, width, height, chunk.bitmap.data()) == 0)
        {
            cLog::Error("PVRTC4 decompression failed.");
            return false;
        }
        break;
    case Decomp::RGB555:
        convertRgb555toRgb888(chunk.bitmap.data(), src, width * height);
        break;
    }

    return true;
}

bool cFormatPvr::loadPvr3(const uint8_t* data, sChunkData& chunk, sImageInfo& info)
{
    info.formatName = "pvr3";

    auto& header = *reinterpret_cast<const PVRv3TexHeader*>(data);

    auto version = helpers::read_uint32(reinterpret_cast<const uint8_t*>(&header.version));
    if (version != 0x50565203)
    {
        cLog::Error("PVR3 version mismatch: {:#x}.", version);
        return false;
    }

    auto pixelFormat = static_cast<PVR3TexturePixelFormat>(header.pixelFormat);
    auto flags = header.flags;
    auto width = header.width;
    auto height = header.height;

    cLog::Debug("-- PVR3 header");
    cLog::Debug("  Version       : {:#x}", version);
    cLog::Debug("  Pixel format  : {:#x}", static_cast<uint64_t>(pixelFormat));
    cLog::Debug("  Flags         : {:#x}", flags);
    cLog::Debug("  Color space   : {}", header.colorSpace);
    cLog::Debug("  Channel type  : {}", header.channelType);
    cLog::Debug("  Width         : {}", width);
    cLog::Debug("  Height        : {}", height);
    cLog::Debug("  Depth         : {}", header.depth);
    cLog::Debug("  Surfaces      : {}", header.numberOfSurfaces);
    cLog::Debug("  Faces         : {}", header.numberOfFaces);
    cLog::Debug("  Mipmaps       : {}", header.numberOfMipmaps);
    cLog::Debug("  Metadata size : {}", header.metadataLength);

    constexpr uint32_t PVR3_PREMULTIPLIED = 0x02;
    if ((flags & PVR3_PREMULTIPLIED) != 0)
    {
        cLog::Warning("Image has premultiplied alpha.");
    }

    enum class Decomp
    {
        Copy,
        PVRTC2,
        PVRTC4,
        GPU,
    };
    auto decompress = Decomp::Copy;
    GpuDecodeFunc gpuDecoder = nullptr;
    auto bytes = 0u;
    switch (pixelFormat)
    {
    case PVR3TexturePixelFormat::RGBA8888:
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case PVR3TexturePixelFormat::BGRA8888:
        bytes = 4;
        chunk.format = ePixelFormat::BGRA;
        break;
    case PVR3TexturePixelFormat::RGB888:
        bytes = 3;
        chunk.format = ePixelFormat::RGB;
        break;
    case PVR3TexturePixelFormat::RGB565:
        bytes = 2;
        chunk.format = ePixelFormat::RGB565;
        break;
    case PVR3TexturePixelFormat::RGBA4444:
        bytes = 2;
        chunk.format = ePixelFormat::RGBA4444;
        break;
    case PVR3TexturePixelFormat::RGBA5551:
        bytes = 2;
        chunk.format = ePixelFormat::RGBA5551;
        break;
    case PVR3TexturePixelFormat::A8:
        bytes = 1;
        chunk.format = ePixelFormat::Alpha;
        break;
    case PVR3TexturePixelFormat::L8:
        bytes = 1;
        chunk.format = ePixelFormat::Luminance;
        break;
    case PVR3TexturePixelFormat::LA88:
        bytes = 2;
        chunk.format = ePixelFormat::LuminanceAlpha;
        break;
    case PVR3TexturePixelFormat::PVRTC2BPP_RGB:
    case PVR3TexturePixelFormat::PVRTC2BPP_RGBA:
        decompress = Decomp::PVRTC2;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case PVR3TexturePixelFormat::PVRTC4BPP_RGB:
    case PVR3TexturePixelFormat::PVRTC4BPP_RGBA:
        decompress = Decomp::PVRTC4;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;

    // GPU compressed formats — all decode to RGBA
    case PVR3TexturePixelFormat::ETC1:
    case PVR3TexturePixelFormat::ETC2_RGB:
        decompress = Decomp::GPU;
        gpuDecoder = gpu_decode::decodeETC2_RGB;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case PVR3TexturePixelFormat::ETC2_RGBA:
        decompress = Decomp::GPU;
        gpuDecoder = gpu_decode::decodeETC2_RGBA;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case PVR3TexturePixelFormat::ETC2_RGBA1:
        decompress = Decomp::GPU;
        gpuDecoder = gpu_decode::decodeETC2_RGBA1;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case PVR3TexturePixelFormat::EAC_R11_Unsigned:
    case PVR3TexturePixelFormat::EAC_R11_Signed:
        decompress = Decomp::GPU;
        gpuDecoder = gpu_decode::decodeEAC_R11;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case PVR3TexturePixelFormat::EAC_RG11_Unsigned:
    case PVR3TexturePixelFormat::EAC_RG11_Signed:
        decompress = Decomp::GPU;
        gpuDecoder = gpu_decode::decodeEAC_RG11;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case PVR3TexturePixelFormat::BC1:
        decompress = Decomp::GPU;
        gpuDecoder = gpu_decode::decodeBC1;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case PVR3TexturePixelFormat::BC2:
        decompress = Decomp::GPU;
        gpuDecoder = gpu_decode::decodeBC2;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case PVR3TexturePixelFormat::BC3:
        decompress = Decomp::GPU;
        gpuDecoder = gpu_decode::decodeBC3;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case PVR3TexturePixelFormat::BC4:
        decompress = Decomp::GPU;
        gpuDecoder = gpu_decode::decodeBC4;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case PVR3TexturePixelFormat::BC5:
        decompress = Decomp::GPU;
        gpuDecoder = gpu_decode::decodeBC5;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;
    case PVR3TexturePixelFormat::BC7:
        decompress = Decomp::GPU;
        gpuDecoder = gpu_decode::decodeBC7;
        bytes = 4;
        chunk.format = ePixelFormat::RGBA;
        break;

    default:
        cLog::Error("Unsupported PVR3 pixel format: {:#x}.", static_cast<uint64_t>(pixelFormat));
        return false;
    }

    chunk.bpp = bytes * 8;
    info.bppImage = bytes * 8;
    chunk.width = width;
    chunk.height = height;
    chunk.pitch = width * bytes;
    chunk.resizeBitmap(chunk.pitch, chunk.height);
    auto src = data + sizeof(PVRv3TexHeader) + header.metadataLength;

    switch (decompress)
    {
    case Decomp::Copy:
        ::memcpy(chunk.bitmap.data(), src, chunk.bitmap.size());
        break;
    case Decomp::PVRTC2:
        if (pvr::PVRTDecompressPVRTC(src, 1, width, height, chunk.bitmap.data()) == 0)
        {
            cLog::Error("PVRTC2 decompression failed.");
            return false;
        }
        break;
    case Decomp::PVRTC4:
        if (pvr::PVRTDecompressPVRTC(src, 0, width, height, chunk.bitmap.data()) == 0)
        {
            cLog::Error("PVRTC4 decompression failed.");
            return false;
        }
        break;
    case Decomp::GPU:
        gpuDecoder(src, chunk.bitmap.data(), width, height);
        break;
    }

    return true;
}

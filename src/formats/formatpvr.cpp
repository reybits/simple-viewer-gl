/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "formatpvr.h"
#include "common/bitmap_description.h"
#include "common/file.h"
#include "common/helpers.h"
#include "libs/PVRTDecompress.h"
#include "libs/etc1.h"

#include <cstdio>
#include <cstring>
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

    const uint32_t PVR_TEXTURE_FLAG_TYPE_MASK = 0xff;

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

    bool isPvr3(const uint8_t* /*buffer*/, uint32_t size)
    {
        return size >= sizeof(PVRv3TexHeader);
    }

    void decodeEncodedPvr(uint32_t* data, size_t len)
    {
        const int enclen = 1024;
        const int securelen = 512;
        const int distance = 64;

        // check if key was set
        // make sure to call caw_setkey_part() for all 4 key parts
        // CCASSERT(s_uEncryptedPvrKeyParts[0] != 0, "file is encrypted but key part 0 is not set. Did you call ZipUtils::setPvrEncryptionKeyPart(...)?");
        // CCASSERT(s_uEncryptedPvrKeyParts[1] != 0, "CCZ file is encrypted but key part 1 is not set. Did you call ZipUtils::setPvrEncryptionKeyPart(...)?");
        // CCASSERT(s_uEncryptedPvrKeyParts[2] != 0, "CCZ file is encrypted but key part 2 is not set. Did you call ZipUtils::setPvrEncryptionKeyPart(...)?");
        // CCASSERT(s_uEncryptedPvrKeyParts[3] != 0, "CCZ file is encrypted but key part 3 is not set. Did you call ZipUtils::setPvrEncryptionKeyPart(...)?");

        // create long key
        static bool s_bEncryptionKeyIsValid = false;
        static uint32_t s_uEncryptionKey[1024];
        static uint32_t s_uEncryptedPvrKeyParts[4] = { 0, 0, 0, 0 };
        if (s_bEncryptionKeyIsValid == false)
        {
            unsigned int y, p, e;
            unsigned int rounds = 6;
            unsigned int sum = 0;
            unsigned int z = s_uEncryptionKey[enclen - 1];

            do
            {
#define DELTA 0x9e3779b9
#define MX (((z >> 5 ^ y << 2) + (y >> 3 ^ z << 4)) ^ ((sum ^ y) + (s_uEncryptedPvrKeyParts[(p & 3) ^ e] ^ z)))

                sum += DELTA;
                e = (sum >> 2) & 3;

                for (p = 0; p < enclen - 1; p++)
                {
                    y = s_uEncryptionKey[p + 1];
                    z = s_uEncryptionKey[p] += MX;
                }

                y = s_uEncryptionKey[0];
                z = s_uEncryptionKey[enclen - 1] += MX;

            } while (--rounds);

            s_bEncryptionKeyIsValid = true;
        }

        uint32_t b = 0;
        uint32_t i = 0;

        // encrypt first part completely
        for (; i < len && i < securelen; i++)
        {
            data[i] ^= s_uEncryptionKey[b++];

            if (b >= enclen)
            {
                b = 0;
            }
        }

        // encrypt second section partially
        for (; i < len; i += distance)
        {
            data[i] ^= s_uEncryptionKey[b++];

            if (b >= enclen)
            {
                b = 0;
            }
        }
    }

    bool inflateCCZBuffer(const uint8_t* buffer, uint32_t bufferLen, Buffer& bitmap)
    {
        auto& header = *reinterpret_cast<const CCZHeader*>(buffer);

        if (::memcmp(&header, "CCZ", 3) != 0)
        {
            return false;
        }

        // auto& s = header.sig;
        // ::printf("Type '%c%c%c%c'\n", s[0], s[1], s[2], s[3]);

        // verify header
        if (header.sig[3] == '!')
        {
            // verify header version
            auto version = helpers::read_uint16((const uint8_t*)&header.version);
            // ::printf("version: %u\n", (uint32_t)version);
            if (version > 2)
            {
                ::printf("(EE) Unsupported CCZ header format\n");
                return false;
            }

            // verify compression format
            auto compressionType = helpers::read_uint16((const uint8_t*)&header.compressionType);
            // ::printf("compressionType: %u\n", (uint32_t)compressionType);
            if (compressionType != CCZHeader::CompressionType::ZLIB)
            {
                ::printf("(EE) CCZ Unsupported compression method\n");
                return false;
            }
        }
        else if (header.sig[3] == 'p')
        {
            // encrypted ccz file
            // header = (CCZHeader*)buffer;

            // verify header version
            auto version = helpers::read_uint16((const uint8_t*)&header.version);
            // ::printf("version: %u\n", (uint32_t)version);
            if (version > 0)
            {
                ::printf("(EE) Unsupported CCZ header format\n");
                return false;
            }

            // verify compression format
            auto compressionType = helpers::read_uint16((const uint8_t*)&header.compressionType);
            // ::printf("compressionType: %u\n", (uint32_t)compressionType);
            if (compressionType != CCZHeader::CompressionType::ZLIB)
            {
                ::printf("(EE) CCZ Unsupported compression method\n");
                return false;
            }

            // decrypt
            auto ints = (uint32_t*)(buffer + 12);
            auto enclen = (bufferLen - 12) / 4;

            decodeEncodedPvr(ints, enclen);

#if 0
        // verify checksum in debug mode
        unsigned int calculated = checksumPvr(ints, enclen);
        unsigned int required = helpers::read_uint32((uint8_t*)&header.reserved);

        if (calculated != required)
        {
            ::printf("Can't decrypt image file. Is the decryption key valid?");
            return -1;
        }
#endif
        }
        else
        {
            ::printf("(EE) Invalid CCZ file\n");
            return false;
        }

        auto size = helpers::read_uint32((uint8_t*)&header.len);
        // ::printf("size: %u\n", size);

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

        z_stream d_stream; /* decompression stream */
        d_stream.zalloc = nullptr;
        d_stream.zfree = nullptr;
        d_stream.opaque = nullptr;

        d_stream.next_in = (uint8_t*)in;
        d_stream.avail_in = inLength;
        d_stream.next_out = bitmap.data();
        d_stream.avail_out = bufferSize;

        /* window size to hold 256k */
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
                return err;
            }

            // not enough memory ?
            if (err != Z_STREAM_END)
            {
                const auto Factor = 2u;

                bitmap.resize(bufferSize * Factor);

                d_stream.next_out = bitmap.data() + bufferSize;
                d_stream.avail_out = bufferSize;
                bufferSize *= Factor;
            }
        }

        bitmap.resize(bufferSize - d_stream.avail_out);

        return inflateEnd(&d_stream) == Z_OK;
    }

} // namespace

cFormatPvr::cFormatPvr(iCallbacks* callbacks)
    : cFormat(callbacks)
{
}

cFormatPvr::~cFormatPvr()
{
}

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
        // ::printf("CCZ buffer\n");
        return true;
    }

    if (isGZipBuffer(file, buffer))
    {
        // ::printf("GZip buffer\n");
        return true;
    }

    return isPvr2(buffer.data(), buffer.size()) || isPvr3(buffer.data(), buffer.size());
}

bool cFormatPvr::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    cFile file;
    if (!file.open(filename))
    {
        return false;
    }

    desc.size = file.getSize();
    file.seek(0, SEEK_SET);

    Buffer buffer(desc.size);

    if (file.read(buffer.data(), (uint32_t)buffer.size()) != (uint32_t)buffer.size())
    {
        ::printf("(EE) Can't read file!\n");
        return false;
    }

    Buffer unpacked;
    const uint8_t* unpackedData = nullptr;
    uint32_t unpackedSize = 0;

    if (isCCZBuffer(buffer.data(), buffer.size()))
    {
        auto result = inflateCCZBuffer(buffer.data(), buffer.size(), unpacked);
        if (result == false)
        {
            ::printf("(EE) Failed to uncompress CCZ data!\n");
            return false;
        }

        unpackedData = unpacked.data();
        unpackedSize = unpacked.size();
    }
    else if (isGZipBuffer(buffer.data(), buffer.size()))
    {
        auto result = inflateMemory(buffer.data(), buffer.size(), unpacked);
        if (result == false)
        {
            ::printf("(EE) Failed to uncompress GZip data!\n");
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
        m_formatName = "pvr2";

        auto& header = *reinterpret_cast<const PVRv2TexHeader*>(unpackedData);

        auto flags = header.flags;
        auto pixelFormat = static_cast<PVR2TexturePixelFormat>(flags & PVR_TEXTURE_FLAG_TYPE_MASK);
        auto flipped = (flags & (uint32_t)PVR2TextureFlag::VerticalFlip) ? true : false;
        if (flipped)
        {
            ::printf("(WW) Image is flipped. Regenerate it using PVRTexTool\n");
        }

        // auto t = (char*)&header.pvrTag;
        // ::printf("tag: %c%c%c%c\n", t[0], t[1], t[2], t[3]);
        // ::printf("Header length: %u\n", header.headerLength);

        auto width = header.width;
        // ::printf("Width: %u\n", width);

        auto height = header.height;
        // ::printf("Height: %u\n", height);

        // ::printf("Mipmaps: %u\n", helpers::read_uint32((uint8_t*)&header.numMipmaps));
        // ::printf("Flags: %u\n", helpers::read_uint32((uint8_t*)&header.flags));
        // ::printf("Data length: %u\n", helpers::read_uint32((uint8_t*)&header.dataLength));

        // auto bpp = helpers::read_uint32((uint8_t*)&header.bpp);
        // ::printf("BPP: %u\n", bpp);

        // ::printf("Mask R: %u\n", helpers::read_uint32((uint8_t*)&header.bitmaskRed));
        // ::printf("Mask G: %u\n", helpers::read_uint32((uint8_t*)&header.bitmaskGreen));
        // ::printf("Mask B: %u\n", helpers::read_uint32((uint8_t*)&header.bitmaskBlue));
        // ::printf("Mask A: %u\n", helpers::read_uint32((uint8_t*)&header.bitmaskAlpha));
        // ::printf("Tag: %u\n", helpers::read_uint32((uint8_t*)&header.pvrTag));
        // ::printf("Surfaces: %u\n", helpers::read_uint32((uint8_t*)&header.numSurfs));

        enum class Decomp
        {
            Copy,
            PVR,
        };
        auto decopmress = Decomp::Copy;
        auto bytes = 0u;
        switch (pixelFormat)
        {
        case PVR2TexturePixelFormat::RGBA4444:
            bytes = 2;
            desc.format = GL_UNSIGNED_SHORT_4_4_4_4;
            break;
        case PVR2TexturePixelFormat::RGBA5551:
            bytes = 2;
            desc.format = GL_UNSIGNED_SHORT_5_5_5_1;
            break;
        case PVR2TexturePixelFormat::RGBA8888:
            bytes = 4;
            desc.format = GL_RGBA;
            break;
        case PVR2TexturePixelFormat::RGB565:
            bytes = 2;
            desc.format = GL_UNSIGNED_SHORT_5_6_5;
            break;
        case PVR2TexturePixelFormat::RGB555:
            bytes = 2;
            desc.format = GL_UNSIGNED_SHORT_5_5_5_1;
            break;
        case PVR2TexturePixelFormat::RGB888:
            bytes = 3;
            desc.format = GL_RGB;
            break;
        case PVR2TexturePixelFormat::BGRA8888:
            bytes = 4;
            desc.format = GL_BGRA;
            break;
        case PVR2TexturePixelFormat::A8:
            bytes = 1;
            desc.format = GL_ALPHA;
            break;
        case PVR2TexturePixelFormat::PVRTC2BPP_RGBA:
            decopmress = Decomp::PVR;
            bytes = 4;
            desc.format = GL_RGBA;
            break;
        case PVR2TexturePixelFormat::PVRTC4BPP_RGBA:
            decopmress = Decomp::PVR;
            bytes = 4;
            desc.format = GL_RGBA;
            break;

        case PVR2TexturePixelFormat::I8:
        case PVR2TexturePixelFormat::AI88:
        default:
            ::printf("(EE) pvr2 usupported pixelFormat %u\n", static_cast<uint32_t>(pixelFormat));
            return false;
        }

        desc.bpp = bytes * 8;
        desc.bppImage = bytes * 8;
        desc.width = width;
        desc.height = height;
        desc.pitch = width * bytes;
        desc.bitmap.resize(desc.pitch * desc.height);
        auto src = unpackedData + sizeof(PVRv2TexHeader);

        auto result = true;
        switch (decopmress)
        {
        case Decomp::Copy:
            ::memcpy(desc.bitmap.data(), src, desc.bitmap.size());
            break;
        case Decomp::PVR:
            result = pvr::PVRTDecompressPVRTC(src, true, width, height, desc.bitmap.data()) == desc.bitmap.size();
            break;
        }

        return result;
    }
    else if (isPvr3(unpackedData, unpackedSize))
    {
        m_formatName = "pvr3";

        auto& header = *reinterpret_cast<const PVRv3TexHeader*>(unpackedData);

        auto version = helpers::read_uint32((uint8_t*)&header.version);
        if (version == 0x50565203)
        {
            // ::printf("version: 0x%x\n", version);

            auto pixelFormat = static_cast<PVR3TexturePixelFormat>(header.pixelFormat);
            // ::printf("pixelFormat: 0x%llx\n", pixelFormat);

            // auto flags = header.flags;
            // ::printf("flags: 0x%x\n", flags);

            // ::printf("colorSpace: %u\n", header.colorSpace);
            // ::printf("channelType: %u\n", header.channelType);

            auto width = header.width;
            // ::printf("width: %u\n", width);

            auto height = header.height;
            // ::printf("height: %u\n", height);

            // ::printf("depth: %u\n", header.depth);
            // ::printf("surfaces: %u\n", header.numberOfSurfaces);
            // ::printf("faces: %u\n", header.numberOfFaces);
            // ::printf("mipmaps: %u\n", header.numberOfMipmaps);
            // ::printf("metadata size: %u\n", header.metadataLength);

            enum class Decomp
            {
                Copy,
                PVR,
                ETC1
            };
            auto decopmress = Decomp::Copy;
            auto bytes = 0u;
            switch (pixelFormat)
            {
            case PVR3TexturePixelFormat::RGBA8888:
                bytes = 4;
                desc.format = GL_RGBA;
                break;
            case PVR3TexturePixelFormat::BGRA8888:
                bytes = 4;
                desc.format = GL_BGRA;
                break;
            case PVR3TexturePixelFormat::RGB888:
                bytes = 3;
                desc.format = GL_RGB;
                break;
            case PVR3TexturePixelFormat::RGB565:
                bytes = 2;
                desc.format = GL_UNSIGNED_SHORT_5_6_5;
                break;
            case PVR3TexturePixelFormat::RGBA4444:
                bytes = 2;
                desc.format = GL_UNSIGNED_SHORT_4_4_4_4;
                break;
            case PVR3TexturePixelFormat::RGBA5551:
                bytes = 2;
                desc.format = GL_UNSIGNED_SHORT_5_5_5_1;
                break;
            case PVR3TexturePixelFormat::A8:
                bytes = 1;
                desc.format = GL_ALPHA;
                break;
            case PVR3TexturePixelFormat::L8:
                bytes = 1;
                desc.format = GL_LUMINANCE;
                break;
            case PVR3TexturePixelFormat::PVRTC2BPP_RGB:
            case PVR3TexturePixelFormat::PVRTC2BPP_RGBA:
                decopmress = Decomp::PVR;
                bytes = 4;
                desc.format = GL_RGBA;
                break;
            case PVR3TexturePixelFormat::PVRTC4BPP_RGB:
            case PVR3TexturePixelFormat::PVRTC4BPP_RGBA:
                decopmress = Decomp::PVR;
                bytes = 4;
                desc.format = GL_RGBA;
                break;
            case PVR3TexturePixelFormat::ETC1:
                decopmress = Decomp::ETC1;
                bytes = 3;
                desc.format = GL_RGB;
                break;

            default:
                ::printf("(EE) pvr3 usupported pixelFormat %llu\n", static_cast<long long unsigned>(pixelFormat));
                return false;
            }

            desc.bpp = bytes * 8;
            desc.bppImage = bytes * 8;
            desc.width = width;
            desc.height = height;
            desc.pitch = width * bytes;
            desc.bitmap.resize(desc.pitch * desc.height);
            auto src = unpackedData + sizeof(PVRv3TexHeader) + header.metadataLength;

            auto result = true;
            switch (decopmress)
            {
            case Decomp::Copy:
                ::memcpy(desc.bitmap.data(), src, desc.bitmap.size());
                break;
            case Decomp::PVR:
                result = pvr::PVRTDecompressPVRTC(src, true, width, height, desc.bitmap.data()) == desc.bitmap.size();
                break;
            case Decomp::ETC1:
                result = etc1_decode_image(src, static_cast<etc1_byte*>(desc.bitmap.data()), width, height, bytes, desc.pitch) != 0;
                break;
            }

            return result;
        }

        ::printf("(EE) pvr3 version mismatch\n");

        return false;
    }

    ::printf("(EE) unknown pvr!\n");

    return false;
}

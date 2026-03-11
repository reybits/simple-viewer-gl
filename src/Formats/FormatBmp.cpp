/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatBmp.h"
#include "Common/CachedReader.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
#include "Log/Log.h"

#include <cstring>
#include <iterator>

namespace
{
#pragma pack(push, 1)
    struct BmpHeader
    {
        uint8_t id[2];
        uint32_t fileSize;
        uint16_t reserved[2];
        uint32_t bitmapOffset;
    };
#pragma pack(pop)

    // -------------------------------------------------------------------------
    // OS/2 1.x
    // -------------------------------------------------------------------------

#pragma pack(push, 1)
    struct BITMAPCOREHEADER
    {
        uint32_t size;     // size of this header (12 bytes)
        uint16_t width;    // bitmap width in pixels (unsigned 16-bit)
        uint16_t height;   // bitmap height in pixels (unsigned 16-bit)
        uint16_t planes;   // number of color planes, must be 1
        uint16_t bitCount; // number of bits per pixel OS/2 1.x bitmaps are uncompressed and cannot be 16 or 32 bpp
    };
#pragma pack(pop)

    // -------------------------------------------------------------------------
    // Windows
    // -------------------------------------------------------------------------

#pragma pack(push, 1)
    struct BITMAPCOMMON
    {
        uint32_t size;        // size of this header (40 bytes);
        uint32_t width;       // bitmap width in pixels (signed integer)
        uint32_t height;      // bitmap height in pixels (signed integer)
        uint16_t planes;      // number of color planes (must be 1)
        uint16_t bitCount;    // number of bits per pixel, which is the color depth of the image. Typical values are 1, 4, 8, 16, 24 and 32.
        uint32_t compression; // compression method being used. See the next table for a list of possible values
        uint32_t sizeImage;   // image size. This is the size of the raw bitmap data; a dummy 0 can be given for BI_RGB bitmaps.;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    // size of this header (40 bytes)
    struct BITMAPINFOHEADER : BITMAPCOMMON
    {
        uint32_t horiResolution;  // horizontal resolution of the image. (pixel per meter, signed integer)
        uint32_t vertResolution;  // vertical resolution of the image. (pixel per meter, signed integer)
        uint32_t paletteColors;   // number of colors in the color palette, or 0 to default to 2n
        uint32_t importantColors; // number of important colors used, or 0 when every color is important; generally ignored
    };
#pragma pack(pop)

    typedef int32_t FXPT2DOT30;

    struct Xyz // CIEXYZ
    {
        int32_t x; // FXPT2DOT30 x;
        int32_t y; // FXPT2DOT30 y;
        int32_t z; // FXPT2DOT30 z;
    };

    struct XyzTriple // CIEXYZTRIPLE
    {
        Xyz r; // CIEXYZ red;
        Xyz g; // CIEXYZ green;
        Xyz b; // CIEXYZ blue;
    };

#pragma pack(push, 1)
    // size of this header (108 bytes)
    struct BITMAPV4HEADER : BITMAPCOMMON
    {
        uint32_t xPelsPerMeter;
        uint32_t yPelsPerMeter;
        uint32_t clrUsed;
        uint32_t clrImportant;
        uint32_t redMask;
        uint32_t greenMask;
        uint32_t blueMask;
        uint32_t alphaMask;
        uint32_t cSType;
        XyzTriple endpoints; // CIEXYZTRIPLE endpoints;
        uint32_t gammaRed;
        uint32_t gammaGreen;
        uint32_t gammaBlue;
    };
#pragma pack(pop)

#pragma pack(push, 1)
    // size of this header (124 bytes)
    struct BITMAPV5HEADER : BITMAPV4HEADER
    {
        uint32_t intent;
        uint32_t profileData;
        uint32_t profileSize;
        uint32_t reserved;
    };
#pragma pack(pop)

    enum class Version
    {
        Core,

        Info,
        V4,
        V5,

        Unknown,
    };

    Version getVersion(unsigned headerSize)
    {
        if (headerSize == sizeof(BITMAPCOREHEADER))
        {
            return Version::Core;
        }
        else if (headerSize == sizeof(BITMAPINFOHEADER))
        {
            return Version::Info;
        }
        else if (headerSize == sizeof(BITMAPV4HEADER))
        {
            return Version::V4;
        }
        else if (headerSize == sizeof(BITMAPV5HEADER))
        {
            return Version::V5;
        }

        return Version::Unknown;
    }

    enum class Compression
    {
        BI_RGB = 0,       // none                           Most common
        BI_RLE8 = 1,      // RLE 8-bit/pixel                Can be used only with 8-bit/pixel bitmaps
        BI_RLE4 = 2,      // RLE 4-bit/pixel                Can be used only with 4-bit/pixel bitmaps
        BI_BITFIELDS = 3, // OS22XBITMAPHEADER: Huffman 1D  BITMAPV2INFOHEADER: RGB bit field masks,
        //                                BITMAPV3INFOHEADER+: RGBA
        BI_JPEG = 4,           // OS22XBITMAPHEADER: RLE-24      BITMAPV4INFOHEADER+: JPEG image for printing[12]
        BI_PNG = 5,            //                                BITMAPV4INFOHEADER+: PNG image for printing[12]
        BI_ALPHABITFIELDS = 6, // RGBA bit field masks           only Windows CE 5.0 with .NET 4.0 or later
        BI_CMYK = 11,          // none                           only Windows Metafile CMYK[3]
        BI_CMYKRLE8 = 12,      // RLE-8                          only Windows Metafile CMYK
        BI_CMYKRLE4 = 13,      // RLE-4                          only Windows Metafile CMYK
    };

    const char* compressionToName(Compression compression)
    {
        const char* Names[] = {
            "BI_RGB",
            "BI_RLE8",
            "BI_RLE4",
            "BI_BITFIELDS",
            "BI_JPEG",
            "BI_PNG",
            "BI_ALPHABITFIELDS",
            "n/a",
            "n/a",
            "n/a",
            "n/a",
            "BI_CMYK",
            "BI_CMYKRLE8",
            "BI_CMYKRLE4",
        };

        auto idx = (size_t)compression;
        return idx < std::size(Names) ? Names[idx] : "n/a";
    }

    void debugHeader(const BITMAPCOREHEADER& header)
    {
        auto ver = getVersion(header.size);
        if (ver == Version::Core)
        {
            cLog::Debug("-- BITMAPCOREHEADER");
            cLog::Debug("  Header size  : {}", header.size);
            cLog::Debug("  Image width  : {}", header.width);
            cLog::Debug("  Image height : {}", header.height);
            cLog::Debug("  Planes       : {}", header.planes);
            cLog::Debug("  Bit count    : {}", header.bitCount);
        }
    }

    void debugHeader(const BITMAPCOMMON& header)
    {
        cLog::Debug("-- BITMAPCOMMON");
        cLog::Debug("  Header size  : {}", header.size);
        cLog::Debug("  Image width  : {}", header.width);
        cLog::Debug("  Image height : {}", header.height);
        cLog::Debug("  Planes       : {}", header.planes);
        cLog::Debug("  Bit count    : {}", header.bitCount);
        cLog::Debug("  Compression  : {}", compressionToName(static_cast<Compression>(header.compression)));
        cLog::Debug("  Size image   : {}", header.sizeImage);

        auto ver = getVersion(header.size);
        if (ver == Version::Info)
        {
            auto& h = reinterpret_cast<const BITMAPINFOHEADER&>(header);
            cLog::Debug("-- BITMAPINFOHEADER");
            cLog::Debug("  Hori resolution  : {}", h.horiResolution);
            cLog::Debug("  Vert resolution  : {}", h.vertResolution);
            cLog::Debug("  Palette colors   : {}", h.paletteColors);
            cLog::Debug("  Important colors : {}", h.importantColors);
        }
        else
        {
            if (ver == Version::V4 || ver == Version::V5)
            {
                auto& h = reinterpret_cast<const BITMAPV4HEADER&>(header);
                cLog::Debug("-- BITMAPV4HEADER");
                cLog::Debug("  X pels/meter : {}", h.xPelsPerMeter);
                cLog::Debug("  Y pels/meter : {}", h.yPelsPerMeter);
                cLog::Debug("  Colors used  : {}", h.clrUsed);
                cLog::Debug("  Colors import: {}", h.clrImportant);
                cLog::Debug("  Red mask     : {}", h.redMask);
                cLog::Debug("  Green mask   : {}", h.greenMask);
                cLog::Debug("  Blue mask    : {}", h.blueMask);
                cLog::Debug("  Alpha mask   : {}", h.alphaMask);
                cLog::Debug("  CS type      : {}", h.cSType);
                cLog::Debug("  Endp red     : {} {} {}", h.endpoints.r.x, h.endpoints.r.y, h.endpoints.r.z);
                cLog::Debug("  Endp green   : {} {} {}", h.endpoints.g.x, h.endpoints.g.y, h.endpoints.g.z);
                cLog::Debug("  Endp blue    : {} {} {}", h.endpoints.b.x, h.endpoints.b.y, h.endpoints.b.z);
                cLog::Debug("  Gamma red    : {}", h.gammaRed);
                cLog::Debug("  Gamma green  : {}", h.gammaGreen);
                cLog::Debug("  Gamma blue   : {}", h.gammaBlue);
            }

            if (ver == Version::V5)
            {
                auto& h = reinterpret_cast<const BITMAPV5HEADER&>(header);
                cLog::Debug("-- BITMAPV5HEADER");
                cLog::Debug("  Intent       : {}", h.intent);
                cLog::Debug("  Profile data : {}", h.profileData);
                cLog::Debug("  Profile size : {}", h.profileSize);
            }
        }
        (void)header;
    }

    bool isValidFormat(const BmpHeader& header, uint32_t size)
    {
        cLog::Debug("File size: {}.", size);
        cLog::Debug("BMP size: {}.", header.fileSize);

        const bool idValid = (header.id[0] == 'B' && header.id[1] == 'M') || // BM – Windows 3.1x, 95, NT, ... etc.
            (header.id[0] == 'B' && header.id[1] == 'A') ||                  // BA – OS/2 struct bitmap array
            (header.id[0] == 'C' && header.id[1] == 'I') ||                  // CI – OS/2 struct color icon
            (header.id[0] == 'C' && header.id[1] == 'P') ||                  // CP – OS/2 const color pointer
            (header.id[0] == 'I' && header.id[1] == 'C') ||                  // IC – OS/2 struct icon
            (header.id[0] == 'P' && header.id[1] == 'T');                    // PT – OS/2 pointer
        return idValid && size == header.fileSize && header.bitmapOffset < size;
    }

    void setGLformat(uint32_t bitCount, sChunkData& chunk, sImageInfo& info)
    {
        info.bppImage = bitCount;

        switch (bitCount)
        {
        case 1:
            chunk.format = ePixelFormat::BGRA; // GL_LUMINANCE;
            chunk.bpp = 32;                    // 8;
            break;

        case 8:
            chunk.format = ePixelFormat::BGRA; // GL_BGR;
            chunk.bpp = 32;                    // 24;
            break;

        case 16:
            chunk.format = ePixelFormat::RGB565;
            chunk.bpp = 16;
            break;

        case 32:
            chunk.format = ePixelFormat::BGRA;
            chunk.bpp = 32;
            break;

        default:
            chunk.format = ePixelFormat::BGR;
            chunk.bpp = 24;
        }
    }

    struct RGBA
    {
        uint8_t r, g, b, a;

        uint32_t toRGBA() const
        {
            return ((uint32_t)a << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }

        uint32_t toBGRA() const
        {
            return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
        }

        uint32_t toRGB() const
        {
            return ((uint32_t)255 << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }

        uint32_t toBGR() const
        {
            return ((uint32_t)255 << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
        }
    };

    bool readUncompressed1(cFile& file, sChunkData& chunk, const BITMAPCOMMON& header, const Buffer& pal)
    {
        const uint32_t inPitch = header.sizeImage / chunk.height;
        Buffer buffer(inPitch);

        auto in = buffer.data();
        auto palette = (RGBA*)pal.data();

        for (uint32_t row = 0; row < chunk.height; row++)
        {
            if (inPitch != file.read(in, inPitch))
            {
                return false;
            }

            auto out = (uint32_t*)(chunk.bitmap.data() + (chunk.height - row - 1) * chunk.pitch);
            size_t idx = 0;
            for (uint32_t i = 0; i < inPitch; i++)
            {
                auto byte = in[i];
                for (uint32_t b = 0; b < 8 && idx < chunk.width; b++, idx++)
                {
                    const uint32_t val = byte & 0x80;
                    auto color = palette[val ? 1 : 0];
                    out[idx] = color.toBGR();

                    byte <<= 1;
                }
            }
        }

        return true;
    }

    bool readRLE8(cFile& file, sChunkData& chunk, const BITMAPCOMMON& header, const Buffer& pal, bool isRle8)
    {
        (void)header;

        cCachedReader reader(file, 5);

        auto palette = (const RGBA*)pal.data();

        size_t ofs = 0;
        const auto pitch = chunk.width;
        const auto start = (uint32_t*)chunk.bitmap.data();
        const auto end = start + (chunk.height * pitch);
        auto bits = end - pitch;

#define COPY_PIXEL(x)                     \
    do                                    \
    {                                     \
        auto spot = &bits[ofs++];         \
        if (spot >= start && spot < end)  \
        {                                 \
            *spot = (palette[x].toBGR()); \
        }                                 \
        else                              \
        {                                 \
            cLog::Warning("Out of bitmap bounds.");  \
        }                                 \
    } while (0)

        while (true)
        {
            uint8_t ch;
            if (!reader.read(&ch, 1))
            {
                return false;
            }

            // encoded mode starts with a run length, and then a byte
            // with two colour indexes to alternate between for the run
            if (ch)
            {
                uint8_t pixel;
                if (!reader.read(&pixel, 1))
                {
                    return false;
                }

                if (isRle8)
                { // 256-color bitmap, compressed
                    do
                    {
                        COPY_PIXEL(pixel);
                    } while (--ch);
                }
                else
                { // 16-color bitmap, compressed
                    uint8_t pixel0 = pixel >> 4;
                    uint8_t pixel1 = pixel & 0x0F;
                    while (true)
                    {
                        COPY_PIXEL(pixel0); // even count, high nibble
                        if (!--ch)
                        {
                            break;
                        }

                        COPY_PIXEL(pixel1); // odd count, low nibble
                        if (!--ch)
                        {
                            break;
                        }
                    }
                }
            }
            else
            {
                // A leading zero is an escape; it may signal the end of the bitmap,
                // a cursor move, or some absolute data.
                // zero tag may be absolute mode or an escape
                if (!reader.read(&ch, 1))
                {
                    return false;
                }

                switch (ch)
                {
                case 0: // end of line
                    ofs = 0;
                    bits -= pitch; // go to previous
                    break;

                case 1:          // end of bitmap
                    return true; // success!

                case 2: // delta
                    if (!reader.read(&ch, 1))
                    {
                        return false;
                    }
                    ofs += ch;

                    if (!reader.read(&ch, 1))
                    {
                        return false;
                    }
                    bits -= (ch * pitch);
                    break;

                default: // no compression
                {
                    bool needsPad = false;

                    if (isRle8)
                    {
                        needsPad = (ch & 1) != 0;
                        do
                        {
                            uint8_t pixel;
                            if (!reader.read(&pixel, 1))
                            {
                                return false;
                            }

                            COPY_PIXEL(pixel);
                        } while (--ch);
                    }
                    else
                    {
                        needsPad = (((ch + 1) >> 1) & 1) != 0; // (ch+1)>>1: bytes size
                        while (true)
                        {
                            uint8_t pixel;
                            if (!reader.read(&pixel, 1))
                            {
                                return false;
                            }

                            COPY_PIXEL(pixel >> 4);
                            if (!--ch)
                            {
                                break;
                            }

                            COPY_PIXEL(pixel & 0x0F);
                            if (!--ch)
                            {
                                break;
                            }
                        }
                    }

                    // pad at even boundary
                    if (needsPad && !reader.read(&ch, 1))
                    {
                        return false;
                    }
                }
                break;
                }
            }
        }

        return false;
    }

    bool readUncompressed8(cFile& file, sChunkData& chunk, const BITMAPCOMMON& header, const Buffer& pal)
    {
        const uint32_t inPitch = header.sizeImage / chunk.height;
        Buffer buffer(inPitch);

        auto in = buffer.data();
        auto palette = (RGBA*)pal.data();

        for (uint32_t row = 0; row < chunk.height; row++)
        {
            if (inPitch != file.read(in, inPitch))
            {
                return false;
            }

            auto out = (uint32_t*)(chunk.bitmap.data() + (chunk.height - row - 1) * chunk.pitch);
            size_t idx = 0;
            for (uint32_t i = 0; i < inPitch; i++)
            {
                const auto byte = in[i];
                auto color = palette[byte];
                out[idx++] = color.toBGR();
            }
        }

        return true;
    }

    bool readUncompressed16(cFile& file, sChunkData& chunk)
    {
        for (uint32_t row = 0; row < chunk.height; row++)
        {
            auto out = chunk.bitmap.data() + (chunk.height - row - 1) * chunk.pitch;
            if (file.read(out, chunk.pitch) != chunk.pitch)
            {
                return false;
            }
        }
        return true;
    }

    bool readUncompressed24(cFile& file, sChunkData& chunk)
    {
        for (uint32_t row = 0; row < chunk.height; row++)
        {
            auto out = chunk.bitmap.data() + (chunk.height - row - 1) * chunk.pitch;
            if (file.read(out, chunk.pitch) != chunk.pitch)
            {
                return false;
            }
        }
        return true;
    }

    bool readUncompressed32(cFile& file, sChunkData& chunk)
    {
        for (uint32_t row = 0; row < chunk.height; row++)
        {
            auto out = chunk.bitmap.data() + (chunk.height - row - 1) * chunk.pitch;
            if (file.read(out, chunk.pitch) != chunk.pitch)
            {
                return false;
            }
        }
        return true;
    }

} // namespace

bool cFormatBmp::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, sizeof(BmpHeader)))
    {
        return false;
    }

    auto bmpHeader = reinterpret_cast<const BmpHeader*>(buffer.data());
    return isValidFormat(*bmpHeader, file.getSize());
}

bool cFormatBmp::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (!openFile(file, filename, info))
    {
        return false;
    }

    cLog::Debug("-- Header struct sizes");
    cLog::Debug("  BITMAPCOREHEADER : {}", sizeof(BITMAPCOREHEADER));
    cLog::Debug("  BITMAPCOMMON     : {}", sizeof(BITMAPCOMMON));
    cLog::Debug("  BITMAPINFOHEADER : {}", sizeof(BITMAPINFOHEADER));
    cLog::Debug("  BITMAPV4HEADER   : {}", sizeof(BITMAPV4HEADER));
    cLog::Debug("  BITMAPV5HEADER   : {}", sizeof(BITMAPV5HEADER));

    BmpHeader bmpHeader;
    if (file.read(&bmpHeader, sizeof(bmpHeader)) != sizeof(bmpHeader))
    {
        cLog::Error("Can't read BMP header.");
        return false;
    }

    if (isValidFormat(bmpHeader, file.getSize()) == false)
    {
        cLog::Error("Invalid BMP header.");
        return false;
    }

    auto offset = file.getOffset();
    BITMAPCOMMON common;
    if (file.read(&common, sizeof(common)) != sizeof(common))
    {
        cLog::Error("Can't read DIB header size.");
        return false;
    }
    file.seek(offset, SEEK_SET);

    auto ver = getVersion(common.size);

    if (ver == Version::Info
        || ver == Version::V4
        || ver == Version::V5)
    {
        BITMAPV5HEADER header;
        if (file.read(&header, common.size) != common.size)
        {
            cLog::Error("Can't read DIB header.");
            return false;
        }

        debugHeader(header);

        chunk.width = header.width;
        chunk.height = header.height;

        setGLformat(header.bitCount, chunk, info);

        setupBitmap(chunk, info, chunk.bpp, chunk.format, "bmp");

        const uint32_t fileOffset = file.getOffset();
        cLog::Debug("  File offset      : {}", fileOffset);
        cLog::Debug("  Bitmap offset    : {}", bmpHeader.bitmapOffset);

        Buffer palette(bmpHeader.bitmapOffset - fileOffset);
        file.read(palette.data(), palette.size());
        cLog::Debug("  Palette data size: {}", palette.size());
        cLog::Debug("  Colors important : {}", header.clrImportant);
        cLog::Debug("  Colors used      : {}", header.clrUsed);

        // file.seek(bmpHeader.bitmapOffset, SEEK_SET);

        const auto compression = (Compression)header.compression;

        if (compression == Compression::BI_RGB || compression == Compression::BI_BITFIELDS)
        {
            bool result = false;

            switch (header.bitCount)
            {
            case 1:
                result = readUncompressed1(file, chunk, header, palette);
                break;

            case 8:
                result = readUncompressed8(file, chunk, header, palette);
                break;

            case 16:
                result = readUncompressed16(file, chunk);
                break;

            case 24:
                result = readUncompressed24(file, chunk);
                break;

            case 32:
                result = readUncompressed32(file, chunk);
                break;
            }

            if (result == false)
            {
                cLog::Error("Can't read bitmap data.");
                return false;
            }
        }
        else if (compression == Compression::BI_RLE4)
        {
            bool result = false;

            switch (header.bitCount)
            {
            case 8:
                result = readRLE8(file, chunk, header, palette, false);
                break;
            }

            if (result == false)
            {
                cLog::Error("Can't read bitmap data.");
                return false;
            }
        }
        else if (compression == Compression::BI_RLE8)
        {
            bool result = false;

            switch (header.bitCount)
            {
            case 8:
                result = readRLE8(file, chunk, header, palette, true);
                break;
            }

            if (result == false)
            {
                cLog::Error("Can't read bitmap data.");
                return false;
            }
        }
        else
        {
            cLog::Error("Unsupported compression: {}.", compressionToName(compression));
            return false;
        }

        // Extract ICC profile from V5 header (only for 8-bit-per-channel formats)
        if (ver == Version::V5 && header.profileSize > 0 && header.profileData > 0
            && (chunk.format == ePixelFormat::BGR || chunk.format == ePixelFormat::BGRA))
        {
            auto profileOffset = sizeof(BmpHeader) + header.profileData;
            if (profileOffset + header.profileSize <= static_cast<size_t>(file.getSize()))
            {
                Buffer iccBuffer(header.profileSize);
                file.seek(static_cast<long>(profileOffset), SEEK_SET);
                if (file.read(iccBuffer.data(), header.profileSize) == header.profileSize)
                {
                    if (applyIccProfile(chunk, iccBuffer.data(), header.profileSize))
                    {
                        info.formatName = "bmp/icc";
                    }
                }
            }
        }
    }
    else
    {
        if (ver == Version::Core)
        {
            BITMAPCOREHEADER header;
            if (file.read(&header, sizeof(header)) != sizeof(header))
            {
                cLog::Error("Can't read DIB header.");
                return false;
            }

            debugHeader(header);
        }

        cLog::Error("Unsupported BMP header.");

        return false;
    }

    return true;
}

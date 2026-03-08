/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

namespace AGE
{
    enum class Format : unsigned
    {
        // Uncompressed formats
        RGBA8888,
        RGBA5551,
        RGBA4444,
        RGB888,
        RGB565,
        A8,

        LAST_UNCOMPRESSED = A8,

        // GPU-compressed formats
        ASTC_4x4,
        ASTC_6x6,
        ASTC_8x8,
        ETC2_RGB,
        ETC2_RGBA,
        BC1,
        BC3,
        BC7,

        UNKNOWN
    };

    enum class Compression : unsigned
    {
        NONE,
        RLE,
        RLE4,
        ZLIB,
        LZ4,
        LZ4HC,

        Count
    };

    struct Header
    {
        unsigned id;
        Format format;
        Compression compression;
        unsigned w;
        unsigned h;
        unsigned data_size;
    };

    void filEmpty(Header& header);
    bool isValidHeader(const Header& header);
    unsigned getVersion(const Header& header);
    bool isCompressedFormat(Format format);
    Format remapV1Format(unsigned v1Format);

    const char* FormatToStr(Format format);
    const char* CompressionToStr(Compression compression);

} // namespace AGE

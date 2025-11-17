/**********************************************\
*
*  AGE by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

namespace AGE
{
    enum class Format : unsigned
    {
        ALPHA,
        RGB,
        RGBA,

        Count
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
    bool isRawHeader(const Header& header);

    const char* FormatToStr(Format format);
    const char* CompressionToStr(Compression compression);
}

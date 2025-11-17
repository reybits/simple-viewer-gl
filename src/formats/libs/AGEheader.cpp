/**********************************************\
*
*  AGE by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "AGEheader.h"

#include <cassert>
#include <cstring>

namespace AGE
{

const char RawId[] = { 'A', 'G', 'E', 1 };

void filEmpty(Header& header)
{
    memcpy(&header.id, RawId, sizeof(header.id));
}

bool isRawHeader(const Header& header)
{
    return memcmp(&header.id, RawId, sizeof(header.id)) == 0;
}

const char* FormatToStr(Format format)
{
    static const char* Names[] =
    {
        "alpha",
        "rgb",
        "rgba",
    };
    static_assert(sizeof(Names)/sizeof(Names[0]) == (unsigned)Format::Count, "AGE::Formats missmatch");

    return Names[(unsigned)format];
}

const char* CompressionToStr(Compression compression)
{
    static const char* Names[] =
    {
        "none",
        "rle",
        "rle4",
        "zlib",
        "lz4",
        "lz4hc",
    };
    static_assert(sizeof(Names)/sizeof(Names[0]) == (unsigned)Compression::Count, "AGE::Compression missmatch");

    return Names[(unsigned)compression];
}

}

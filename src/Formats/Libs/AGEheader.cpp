/**********************************************\
*
*  AGE by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "AGEheader.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <iterator>

namespace AGE
{

constexpr char RawIdV1[] = { 'A', 'G', 'E', 1 };
constexpr char RawIdV2[] = { 'A', 'G', 'E', 2 };

void filEmpty(Header& header)
{
    memcpy(&header.id, RawIdV2, sizeof(header.id));
}

bool isValidHeader(const Header& header)
{
    return memcmp(&header.id, RawIdV1, sizeof(header.id)) == 0
        || memcmp(&header.id, RawIdV2, sizeof(header.id)) == 0;
}

unsigned getVersion(const Header& header)
{
    return reinterpret_cast<const uint8_t*>(&header.id)[3];
}

bool isCompressedFormat(Format format)
{
    return format > Format::LAST_UNCOMPRESSED && format < Format::UNKNOWN;
}

Format remapV1Format(unsigned v1Format)
{
    // v1 enum: 0=ALPHA, 1=RGB, 2=RGBA
    switch (v1Format)
    {
    case 0:
        return Format::A8;
    case 1:
        return Format::RGB888;
    case 2:
        return Format::RGBA8888;
    default:
        return Format::UNKNOWN;
    }
}

const char* FormatToStr(Format format)
{
    static const char* Names[] =
    {
        "rgba8888",
        "rgba5551",
        "rgba4444",
        "rgb888",
        "rgb565",
        "a8",
        "astc_4x4",
        "astc_6x6",
        "astc_8x8",
        "etc2_rgb",
        "etc2_rgba",
        "bc1",
        "bc3",
        "bc7",
    };
    static_assert(std::size(Names) == static_cast<size_t>(Format::UNKNOWN), "AGE::Format mismatch");

    auto idx = static_cast<size_t>(format);
    if (idx < std::size(Names))
    {
        return Names[idx];
    }

    return "unknown";
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
    static_assert(std::size(Names) == static_cast<size_t>(Compression::Count), "AGE::Compression mismatch");

    auto idx = static_cast<size_t>(compression);
    if (idx < std::size(Names))
    {
        return Names[idx];
    }

    return "unknown";
}

}

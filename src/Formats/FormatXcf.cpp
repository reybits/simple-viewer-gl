/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatXcf.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
#include "Libs/Xcf.h"

#include <cstring>

bool cFormatXcf::isSupported(cFile& file, Buffer& buffer) const
{
    constexpr char Header[] = { 'g', 'i', 'm', 'p', ' ', 'x', 'c', 'f' };

    if (readBuffer(file, buffer, sizeof(Header)) == false)
    {
        return false;
    }

    return std::memcmp(buffer.data(), Header, sizeof(Header)) == 0;
}

bool cFormatXcf::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (openFile(file, filename, info) == false)
    {
        return false;
    }

    info.formatName = "xcf";

    return xcf::import(file, chunk, info);
}

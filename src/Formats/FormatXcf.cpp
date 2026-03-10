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

#include <cstdio>
#include <cstring>

bool cFormatXcf::isSupported(cFile& file, Buffer& buffer) const
{
    const char header[8] = { 'g', 'i', 'm', 'p', ' ', 'x', 'c', 'f' };

    if (!readBuffer(file, buffer, sizeof(header)))
    {
        return false;
    }

    return ::memcmp(buffer.data(), header, sizeof(header)) == 0;
}

bool cFormatXcf::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (!openFile(file, filename, info))
    {
        return false;
    }

    info.formatName = "xcf";

    return import_xcf(file, chunk, info);
}

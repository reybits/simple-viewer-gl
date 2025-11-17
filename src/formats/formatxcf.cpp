/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "formatxcf.h"
#include "common/bitmap_description.h"
#include "common/file.h"
#include "libs/xcf.h"

#include <cstdio>
#include <cstring>

cFormatXcf::cFormatXcf(iCallbacks* callbacks)
    : cFormat(callbacks)
{
}

cFormatXcf::~cFormatXcf()
{
}

bool cFormatXcf::isSupported(cFile& file, Buffer& buffer) const
{
    const char header[8] = { 'g', 'i', 'm', 'p', ' ', 'x', 'c', 'f' };

    if (!readBuffer(file, buffer, sizeof(header)))
    {
        return false;
    }

    return ::memcmp(buffer.data(), header, sizeof(header)) == 0;
}

bool cFormatXcf::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    cFile file;
    if (file.open(filename) == false)
    {
        return false;
    }

    m_formatName = "xcf";

    return import_xcf(file, desc);
}

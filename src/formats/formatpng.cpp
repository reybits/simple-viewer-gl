/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "formatpng.h"
#include "common/file.h"
#include "libs/PngReader.h"

#include <cstring>

cFormatPng::cFormatPng(iCallbacks* callbacks)
    : cFormat(callbacks)
{
}

cFormatPng::~cFormatPng()
{
}

bool cFormatPng::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, cPngReader::HeaderSize))
    {
        return false;
    }

    return cPngReader::isValid(buffer.data(), file.getSize());
}

bool cFormatPng::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    cFile file;
    if (!file.open(filename))
    {
        return false;
    }

    cPngReader reader;
    reader.setProgressCallback([this](float progress) {
        updateProgress(progress);
    });

    m_formatName = "png";

    auto result = reader.loadPng(desc, file);
    if (result)
    {
        auto& iccProfile = reader.getIccProfile();
        if (iccProfile.size() != 0)
        {
            if (applyIccProfile(desc, iccProfile.data(), iccProfile.size()))
            {
                m_formatName = "png/icc";
            }
        }
    }

    return result;
}

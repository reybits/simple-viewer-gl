/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatPng.h"
#include "Common/BitmapDescription.h"
#include "Common/File.h"
#include "Libs/PngReader.h"

#include <cstring>

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
    if (!openFile(file, filename, desc))
    {
        return false;
    }

    cPngReader reader;
    reader.setProgressCallback([this](float progress) {
        updateProgress(progress);
    });
    reader.setBitmapAllocatedCallback([this]() {
        signalBitmapAllocated();
    });

    desc.formatName = "png";

    auto result = reader.loadPng(desc, file);
    if (result)
    {
        auto& iccProfile = reader.getIccProfile();
        if (applyIccProfile(desc, iccProfile.data(), static_cast<uint32_t>(iccProfile.size())))
        {
            desc.formatName = "png/icc";
        }
    }

    return result;
}

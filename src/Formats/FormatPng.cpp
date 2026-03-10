/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatPng.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
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

bool cFormatPng::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (!openFile(file, filename, info))
    {
        return false;
    }

    cPngReader reader;
    reader.setProgressCallback([this](float progress) {
        updateProgress(progress);
    });
    reader.setBitmapAllocatedCallback([this, &reader, &info]() {
        info.formatName = reader.getIccProfile().empty() ? "png" : "png/icc";
        signalBitmapAllocated();
    });
    reader.setStopFlag(&m_stop);

    // ICC LUT generated inside loadPng() — applied on GPU during rendering
    return reader.loadPng(chunk, info, file);
}

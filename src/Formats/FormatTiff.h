/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#if defined(TIFF_SUPPORT)

#include "Format.h"

#include <string>

class cFormatTiff final : public cFormat
{
public:
    using cFormat::cFormat;

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sBitmapDescription& desc) override;
    bool LoadSubImageImpl(unsigned current, sBitmapDescription& desc) override;

private:
    bool load(unsigned current, sBitmapDescription& desc);

private:
    std::string m_filename;
};

#endif

/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#if defined(TIFF_SUPPORT)

#include "format.h"

#include <string>

class cFormatTiff final : public cFormat
{
public:
    explicit cFormatTiff(sCallbacks* callbacks);
    ~cFormatTiff();

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

/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#if defined(JPEG2000_SUPPORT)

#include "Format.h"

class cFormatJp2k final : public cFormat
{
public:
    using cFormat::cFormat;

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sBitmapDescription& desc) override;

    bool preAllocateBitmap(void* image, sBitmapDescription& desc);
    bool convertPixels(void* image, sBitmapDescription& desc);
};

#endif

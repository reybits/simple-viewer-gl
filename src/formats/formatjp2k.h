/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#if defined(JPEG2000_SUPPORT)

#include "format.h"

class cFormatJp2k final : public cFormat
{
public:
    explicit cFormatJp2k(iCallbacks* callbacks);
    ~cFormatJp2k();

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sBitmapDescription& desc) override;

    bool preAllocateBitmap(void* image, sBitmapDescription& desc);
    bool convertPixels(void* image, sBitmapDescription& desc);
};

#endif

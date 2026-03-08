/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include "jpegdecoder.h"

class cFormatJpeg final : public cJpegDecoder
{
public:
    explicit cFormatJpeg(sCallbacks* callbacks);
    ~cFormatJpeg();

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sBitmapDescription& desc) override;
};

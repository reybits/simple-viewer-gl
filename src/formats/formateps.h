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

class cFormatEps final : public cJpegDecoder
{
public:
    explicit cFormatEps(sCallbacks* callbacks);
    ~cFormatEps();

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sBitmapDescription& desc) override;
};

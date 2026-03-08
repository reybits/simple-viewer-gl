/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#if defined(OPENEXR_SUPPORT)

#include "format.h"

class cFormatExr final : public cFormat
{
public:
    explicit cFormatExr(sCallbacks* callbacks);
    ~cFormatExr();

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sBitmapDescription& desc) override;
};

#endif

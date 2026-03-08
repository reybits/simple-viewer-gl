/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#if defined(WEBP_SUPPORT)

#include "format.h"

class cFormatWebP final : public cFormat
{
public:
    explicit cFormatWebP(sCallbacks* callbacks);
    ~cFormatWebP();

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sBitmapDescription& desc) override;
};

#endif

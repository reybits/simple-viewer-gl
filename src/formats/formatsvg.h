/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include "format.h"

struct NSVGrasterizer;

class cFormatSvg final : public cFormat
{
public:
    explicit cFormatSvg(sCallbacks* callbacks);
    ~cFormatSvg();

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sBitmapDescription& desc) override;

private:
    NSVGrasterizer* m_rasterizer;
};

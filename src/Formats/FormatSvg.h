/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Format.h"

class cFormatSvg final : public cFormat
{
public:
    explicit cFormatSvg(sCallbacks* callbacks);
    ~cFormatSvg();

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info) override;
};

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
    bool LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info) override;

    void decodePreview(cFile& file, long fileSize, uint32_t reduceFactor, uint32_t fullWidth, uint32_t fullHeight);
    bool preAllocateBitmap(void* image, sChunkData& chunk, sImageInfo& info);
    bool convertPixels(void* image, sChunkData& chunk, uint32_t dstX, uint32_t dstY);
};

#endif

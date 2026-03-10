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

struct opj_image;
typedef opj_image opj_image_t;

class cFormatJp2k final : public cFormat
{
public:
    using cFormat::cFormat;

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info) override;

    void decodePreview(cFile& file, long fileSize, uint32_t reduceFactor, uint32_t fullWidth, uint32_t fullHeight);
    void convertPixels(opj_image_t* image, sChunkData& chunk, uint32_t dstX, uint32_t dstY);
};

#endif

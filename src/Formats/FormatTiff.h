/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#if defined(TIFF_SUPPORT)

#include "Format.h"

#include <string>

class cFormatTiff final : public cFormat
{
public:
    using cFormat::cFormat;

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info) override;
    bool LoadSubImageImpl(uint32_t current, sChunkData& chunk, sImageInfo& info) override;

private:
    bool load(uint32_t current, sChunkData& chunk, sImageInfo& info);
    void decodePreview(void* tif, uint32_t fullWidth, uint32_t fullHeight, unsigned current);

private:
    std::string m_filename;
};

#endif

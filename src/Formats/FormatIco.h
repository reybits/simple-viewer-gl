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

#include <string>

class cFile;
struct IcoDirentry;

class cFormatIco final : public cFormat
{
public:
    using cFormat::cFormat;

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info) override;
    bool LoadSubImageImpl(uint32_t current, sChunkData& chunk, sImageInfo& info) override;

    bool load(uint32_t current, sChunkData& chunk, sImageInfo& info);
    bool loadOrdinaryFrame(sChunkData& chunk, sImageInfo& info, cFile& file, const IcoDirentry* image);
    bool loadPngFrame(sChunkData& chunk, sImageInfo& info, cFile& file, const IcoDirentry* image);

    std::string m_filename;
};

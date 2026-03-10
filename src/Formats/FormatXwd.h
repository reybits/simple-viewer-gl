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

struct X10WindowDump;
struct X11WindowDump;

class cFile;

class cFormatXwd final : public cFormat
{
public:
    using cFormat::cFormat;

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info) override;

private:
    bool loadX10(const X10WindowDump& header, cFile& file, sChunkData& chunk, sImageInfo& info);
    bool loadX11(const X11WindowDump& header, cFile& file, sChunkData& chunk, sImageInfo& info);
};

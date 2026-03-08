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

class cFileInterface;

class cFormatPvr final : public cFormat
{
public:
    using cFormat::cFormat;

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sBitmapDescription& desc) override;

    bool isGZipBuffer(const uint8_t* buffer, uint32_t size) const;
    bool isGZipBuffer(cFile& file, Buffer& buffer) const;

    bool isCCZBuffer(cFile& file, Buffer& buffer) const;
    bool isCCZBuffer(const uint8_t* buffer, uint32_t size) const;
};

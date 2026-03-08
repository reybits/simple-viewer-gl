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
    virtual bool LoadImpl(const char* filename, sBitmapDescription& desc) override;
    virtual bool LoadSubImageImpl(unsigned current, sBitmapDescription& desc) override;

private:
    bool load(uint32_t current, sBitmapDescription& desc);
    bool loadOrdinaryFrame(sBitmapDescription& desc, cFile& file, const IcoDirentry* image);
    bool loadPngFrame(sBitmapDescription& desc, cFile& file, const IcoDirentry* image);
    int calcIcoPitch(uint32_t bppImage, uint32_t width);
    uint32_t getBit(const uint8_t* data, uint32_t bit, uint32_t width);
    uint32_t getNibble(const uint8_t* data, uint32_t nibble, uint32_t width);
    uint32_t getByte(const uint8_t* data, uint32_t byte, uint32_t width);

private:
    std::string m_filename;
};

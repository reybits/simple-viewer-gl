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

class cFileInterface;

class cFormatPvr final : public cFormat
{
public:
    explicit cFormatPvr(sCallbacks* callbacks);
    ~cFormatPvr();

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sBitmapDescription& desc) override;

    bool isGZipBuffer(const uint8_t* buffer, uint32_t size) const;
    bool isGZipBuffer(cFile& file, Buffer& buffer) const;

    bool isCCZBuffer(cFile& file, Buffer& buffer) const;
    bool isCCZBuffer(const uint8_t* buffer, uint32_t size) const;
};

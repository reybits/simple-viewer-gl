/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Formats/Format.h"

struct jpeg_decompress_struct;

class cJpegDecoder : public cFormat
{
public:
    explicit cJpegDecoder(sCallbacks* callbacks);

protected:
    bool decodeJpeg(const uint8_t* in, uint32_t size, sBitmapDescription& desc);

private:
    void setupMarkers(jpeg_decompress_struct* cinfo);

    using Icc = std::vector<uint8_t>;

    bool locateICCProfile(const jpeg_decompress_struct& cinfo, Icc& icc) const;

private:
    const uint8_t JPEG_EXIF; // Exif/XMP
    const uint8_t JPEG_ICCP; // ICC profile
};

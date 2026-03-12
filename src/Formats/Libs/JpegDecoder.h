/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Common/Buffer.h"
#include "Common/PixelFormat.h"

#include <cstdint>
#include <functional>
#include <vector>

struct jpeg_decompress_struct;
struct sChunkData;
struct sImageInfo;

class cJpegDecoder final
{
public:
    struct Bitmap
    {
        Buffer data;
        uint32_t width      = 0;
        uint32_t height     = 0;
        uint32_t pitch      = 0;
        uint32_t bpp        = 0;
        ePixelFormat format = ePixelFormat::RGB;
    };

    struct Result
    {
        bool success = false;
        std::vector<uint8_t> iccProfile;
        std::vector<uint8_t> exifData;
    };

    using ProgressCallback  = std::function<void(float)>;
    using AllocatedCallback = std::function<void()>;
    using ImageInfoCallback = std::function<void()>;
    using PreviewCallback   = std::function<void(Bitmap&&)>;

    Result decodeJpeg(const uint8_t* in, uint32_t size, sChunkData& chunk, sImageInfo& info,
                      const ProgressCallback& onProgress, const AllocatedCallback& onAllocated,
                      const ImageInfoCallback& onImageInfo, const PreviewCallback& onPreview,
                      const bool& stop);

    static Bitmap decodeThumbnail(const uint8_t* in, uint32_t size);

private:
    static void setupMarkers(jpeg_decompress_struct* cinfo);
    static bool locateICCProfile(const jpeg_decompress_struct& cinfo, std::vector<uint8_t>& icc);
    static bool locateExifData(const jpeg_decompress_struct& cinfo, std::vector<uint8_t>& exif);
    static bool locateExifThumbnail(const std::vector<uint8_t>& exif, const uint8_t*& jpegData, uint32_t& jpegSize);

private:
    static constexpr uint8_t JPEG_EXIF = 0xe1; // JPEG_APP0 + 1: Exif/XMP
    static constexpr uint8_t JPEG_ICCP = 0xe2; // JPEG_APP0 + 2: ICC profile
};

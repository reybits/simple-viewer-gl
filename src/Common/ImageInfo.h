/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct sImageInfo
{
    uint32_t bppImage = 0;            // bit per pixel of original image
    long fileSize = -1;               // file size on disk
    const char* formatName = nullptr; // format identifier (e.g. "png", "jpeg/icc")

    uint32_t images = 0;
    uint32_t current = 0;

    bool isAnimation = false;
    uint32_t delay = 0; // frame animation delay

    enum class ExifCategory : uint8_t
    {
        Camera,
        Exposure,
        Image,
        Date,
        Software,
        Info,
        Other,

        Count,
    };

    struct ExifEntry
    {
        ExifCategory category = ExifCategory::Other;
        std::string tag;
        std::string value;
    };
    using ExifList = std::vector<ExifEntry>;
    ExifList exifList;

    uint16_t exifOrientation = 1; // EXIF orientation tag (1-8), 1 = normal
};

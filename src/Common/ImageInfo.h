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
    bool isVector = false; // vector format that supports re-rasterization at different sizes
    uint32_t delay = 0; // frame animation delay

    enum class ExifCategory : uint8_t
    {
        Camera,
        Exposure,
        Image,
        Date,
        GPS,
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

    // EXIF orientation (values match the standard EXIF tag 0x0112).
    enum class Orientation : uint16_t
    {
        Normal     = 1,
        FlipH      = 2,
        Rotate180  = 3,
        FlipV      = 4,
        Transpose  = 5, // mirror across the main diagonal
        Rotate90   = 6, // clockwise
        Transverse = 7, // mirror across the anti-diagonal
        Rotate270  = 8, // clockwise
    };
    Orientation exifOrientation = Orientation::Normal;
};

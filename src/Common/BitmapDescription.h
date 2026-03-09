/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Buffer.h"
#include "Helpers.h"
#include "PixelFormat.h"

#include <atomic>
#include <string>

struct sBitmapDescription
{
    void reset()
    {
        bitmap.clear();
        bitmapSize = 0;
        format = ePixelFormat::RGB;
        bpp = 0;
        pitch = 0;
        width = 0;
        height = 0;

        bppImage = 0;
        size = -1;

        isCompressedTexture = false;
        compressedSize = 0;

        images = 0;
        current = 0;

        isAnimation = false;
        delay = 0;

        formatName = nullptr;

        readyHeight.store(0, std::memory_order_relaxed);

        exifList.clear();
        exifOrientation = 1;
    }

    // Safe bitmap resize — prevents uint32 overflow for large images.
    void resizeBitmap(uint32_t p, uint32_t h)
    {
        bitmap.resize(static_cast<size_t>(p) * h);
    }

    // Allocate bitmap with standard pitch (width * bpp rounded up to bytes).
    // Sets width, height, bpp, format, pitch and resizes the bitmap buffer.
    void allocate(uint32_t w, uint32_t h, uint32_t bitsPerPixel, ePixelFormat fmt)
    {
        width = w;
        height = h;
        bpp = bitsPerPixel;
        format = fmt;
        pitch = helpers::calculatePitch(w, bitsPerPixel);
        resizeBitmap(pitch, h);
    }

    // buffer related
    Buffer bitmap;
    size_t bitmapSize = 0;                  // preserved after bitmap is freed for display purposes
    std::atomic<uint32_t> readyHeight{ 0 }; // rows decoded so far (for progressive upload)
    ePixelFormat format = ePixelFormat::RGB;
    uint32_t bpp = 0;
    uint32_t pitch = 0;
    uint32_t width = 0;
    uint32_t height = 0;

    // file related
    uint32_t bppImage = 0;            // bit per pixel of original image
    long size = -1;                   // file size on disk
    const char* formatName = nullptr; // format identifier (e.g. "png", "jpeg/icc")

    // GPU-compressed texture (ASTC, ETC2, BC)
    bool isCompressedTexture = false;
    uint32_t compressedSize = 0; // size of compressed texture data in bytes

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

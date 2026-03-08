/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include "buffer.h"

#include <glad/glad.h>

#include <atomic>

// Legacy format constants removed in GL 3.3 Core but still used by format readers.
// render::setData() maps these to Core-compatible formats (GL_RED + swizzle).
#ifndef GL_LUMINANCE
#define GL_LUMINANCE 0x1909
#endif
#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA 0x190A
#endif

#include <string>

struct sBitmapDescription
{
    void reset()
    {
        bitmap.clear();
        bitmapSize  = 0;
        format      = GL_RGB;
        bpp         = 0;
        pitch       = 0;
        width       = 0;
        height      = 0;

        bppImage    = 0;
        size        = -1;

        isCompressedTexture = false;
        compressedSize      = 0;

        images      = 0;
        current     = 0;

        isAnimation = false;
        delay       = 0;

        readyHeight.store(0, std::memory_order_relaxed);

        exifList.clear();
    }

    // buffer related
    Buffer bitmap;
    size_t bitmapSize = 0; // preserved after bitmap is freed for display purposes
    std::atomic<uint32_t> readyHeight{ 0 }; // rows decoded so far (for progressive upload)
    GLenum format     = GL_RGB;
    uint32_t bpp      = 0;
    uint32_t pitch    = 0;
    uint32_t width    = 0;
    uint32_t height   = 0;

    // file related
    uint32_t bppImage = 0;  // bit per pixel of original image
    long size         = -1; // file size on disk

    // GPU-compressed texture (ASTC, ETC2, BC)
    bool isCompressedTexture = false;
    uint32_t compressedSize  = 0; // size of compressed texture data in bytes

    uint32_t images   = 0;
    uint32_t current  = 0;

    bool isAnimation  = false;
    uint32_t delay    = 0; // frame animation delay

    struct ExifEntry
    {
        std::string tag;
        std::string value;
    };
    typedef std::vector<ExifEntry> ExifList;
    ExifList exifList;
};

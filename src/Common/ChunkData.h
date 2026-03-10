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
#include <vector>

struct sChunkData
{
    void reset()
    {
        bitmap.clear();
        format = ePixelFormat::RGB;
        bpp = 0;
        pitch = 0;
        width = 0;
        height = 0;
        bandHeight = 0;

        isCompressedTexture = false;
        compressedSize = 0;

        lutData.clear();
        lutSize = 0;

        readyHeight.store(0, std::memory_order_relaxed);
        consumedHeight.store(0, std::memory_order_relaxed);
    }

    // Safe bitmap resize — prevents uint32 overflow for large images.
    void resizeBitmap(uint32_t p, uint32_t h)
    {
        bitmap.resize(static_cast<size_t>(p) * h);
    }

    // Allocate bitmap with standard pitch (width * bpp rounded up to bytes).
    // Sets width, height, bpp, format, pitch and resizes the bitmap buffer.
    // Uses a band buffer of bandRows height (0 = full image).
    void allocate(uint32_t w, uint32_t h, uint32_t bitsPerPixel, ePixelFormat fmt, uint32_t bandRows = 0)
    {
        width = w;
        height = h;
        bpp = bitsPerPixel;
        format = fmt;
        pitch = helpers::calculatePitch(w, bitsPerPixel);
        bandHeight = (bandRows > 0 && bandRows < h) ? bandRows : h;
        resizeBitmap(pitch, bandHeight);
    }

    // Get pointer to row in the band buffer (modular for ring access).
    uint8_t* rowPtr(uint32_t row)
    {
        return bitmap.data() + static_cast<size_t>(row % bandHeight) * pitch;
    }

    const uint8_t* rowPtr(uint32_t row) const
    {
        return bitmap.data() + static_cast<size_t>(row % bandHeight) * pitch;
    }

    // Bitmap buffer
    Buffer bitmap;
    std::atomic<uint32_t> readyHeight{ 0 };    // rows decoded so far (decoder → viewer)
    std::atomic<uint32_t> consumedHeight{ 0 };  // rows uploaded to GPU (viewer → decoder)
    ePixelFormat format = ePixelFormat::RGB;
    uint32_t bpp = 0;
    uint32_t pitch = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t bandHeight = 0; // band buffer height (== height when no banding)

    // GPU-compressed texture (ASTC, ETC2, BC)
    bool isCompressedTexture = false;
    uint32_t compressedSize = 0; // size of compressed texture data in bytes

    // 3D LUT for GPU ICC color correction (33³×3 RGB bytes)
    std::vector<uint8_t> lutData;
    uint32_t lutSize = 0; // grid dimension (e.g. 33)
};

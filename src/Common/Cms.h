/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "PixelFormat.h"

#include <cstdint>

namespace cms
{
    bool transformBitmap(const void* iccProfile, uint32_t iccProfileSize,
                         uint8_t* bitmap, uint32_t width, uint32_t height,
                         uint32_t pitch, ePixelFormat format);

    bool transformBitmap(const float* chr, const float* wp,
                         const uint16_t* gmr, const uint16_t* gmg, const uint16_t* gmb,
                         uint8_t* bitmap, uint32_t width, uint32_t height,
                         uint32_t pitch, ePixelFormat format);

    // Per-scanline ICC transform: create once, apply per row, destroy when done.
    void* createTransform(const void* iccProfile, uint32_t iccProfileSize, ePixelFormat format);
    void transformRow(void* transform, uint8_t* row, uint32_t width);
    void destroyTransform(void* transform);

} // namespace cms

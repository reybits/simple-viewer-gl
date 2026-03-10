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
#include "PixelFormat.h"

struct sBitmap
{
    sBitmap() = default;
    sBitmap(sBitmap&&) = default;
    sBitmap& operator=(sBitmap&&) = default;

    sBitmap(const sBitmap&) = delete;
    sBitmap& operator=(const sBitmap&) = delete;

    Buffer bitmap;
    ePixelFormat format = ePixelFormat::RGB;
    uint32_t bpp = 0;
    uint32_t pitch = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

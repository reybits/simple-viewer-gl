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

#include <functional>

struct sBitmapDescription;

struct sPreviewData
{
    Buffer bitmap;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t pitch = 0;
    uint32_t bpp = 0;
    ePixelFormat format = ePixelFormat::RGB;
    uint32_t fullImageWidth = 0;  // full-resolution dimensions
    uint32_t fullImageHeight = 0;
};

struct sCallbacks
{
    std::function<void()> startLoading;
    std::function<void(const sBitmapDescription& desc)> onImageInfo;
    std::function<void(const sBitmapDescription& desc)> onBitmapAllocated;
    std::function<void(float progress)> doProgress;
    std::function<void()> endLoading;
    std::function<void(sPreviewData&&)> onPreviewReady;
};

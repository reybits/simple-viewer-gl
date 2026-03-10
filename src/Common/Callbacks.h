/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Bitmap.h"

#include <functional>

struct sChunkData;
struct sImageInfo;

struct sPreviewData : sBitmap
{
    uint32_t fullImageWidth = 0;  // full-resolution dimensions
    uint32_t fullImageHeight = 0;
};

struct sCallbacks
{
    std::function<void()> startLoading;
    std::function<void(const sChunkData& chunk, const sImageInfo& info)> onImageInfo;
    std::function<void(const sChunkData& chunk)> onBitmapAllocated;
    std::function<void(float progress)> doProgress;
    std::function<void()> endLoading;
    std::function<void(sPreviewData&&)> onPreviewReady;
};

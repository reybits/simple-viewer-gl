/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include <functional>

struct sBitmapDescription;

struct sCallbacks
{
    std::function<void()> startLoading;
    std::function<void(const sBitmapDescription& desc)> onBitmapAllocated;
    std::function<void(float progress)> doProgress;
    std::function<void()> endLoading;
};

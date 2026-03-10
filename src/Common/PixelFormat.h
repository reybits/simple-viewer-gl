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

enum class ePixelFormat : uint32_t
{
    RGB,
    RGBA,
    BGR,
    BGRA,
    Luminance,
    LuminanceAlpha,
    Alpha,
    RGB565,
    RGBA5551,
    RGBA4444,
    CMYK,
};

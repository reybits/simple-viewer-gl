/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
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
};

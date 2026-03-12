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

// GPU post-processing effect flags.
// Set by format readers in sChunkData::effects, forwarded to the renderer.
// Ordered by shader application: CMYK→RGB, then unpremultiply, then LUT.
enum class eEffect : uint32_t
{
    None          = 0,
    Cmyk          = 1 << 0, // Convert CMYK to RGB
    Unpremultiply = 1 << 1, // Unpremultiply alpha
    Lut           = 1 << 2, // Apply ICC 3D LUT color correction
};

inline eEffect operator|(eEffect a, eEffect b)
{
    return static_cast<eEffect>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline eEffect& operator|=(eEffect& a, eEffect b)
{
    a = a | b;
    return a;
}

inline bool operator&(eEffect a, eEffect b)
{
    return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
}

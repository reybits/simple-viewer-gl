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
#include <vector>

namespace cms
{
    constexpr uint32_t LutGridSize = 33;

    // Generate a 3D LUT (LutGridSize³ × 3 RGB bytes) from an ICC profile.
    // Returns empty vector if the profile is incompatible.
    std::vector<uint8_t> generateLut3D(const void* iccProfile, uint32_t iccProfileSize, ePixelFormat format);

    // Generate a 3D LUT from TIFF chromaticity/whitepoint/transfer-function tables.
    std::vector<uint8_t> generateLut3D(const float* chr, const float* wp,
                                        const uint16_t* gmr, const uint16_t* gmg, const uint16_t* gmb,
                                        ePixelFormat format);

    // Generate a 3D LUT for LAB→sRGB conversion (default D50 whitepoint).
    // Used when LAB image has no embedded ICC profile.
    std::vector<uint8_t> generateLabLut3D();

} // namespace cms

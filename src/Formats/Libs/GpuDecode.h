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

namespace gpu_decode
{
    void decodeBC1(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height);
    void decodeBC2(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height);
    void decodeBC3(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height);
    void decodeBC4(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height);
    void decodeBC5(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height);
    void decodeBC7(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height);
    void decodeETC2_RGB(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height);
    void decodeETC2_RGBA(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height);
    void decodeETC2_RGBA1(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height);
    void decodeEAC_R11(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height);
    void decodeEAC_RG11(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height);
    void decodeASTC(const uint8_t* src, uint8_t* dst, uint32_t width, uint32_t height, uint32_t blockW, uint32_t blockH);

} // namespace gpu_decode

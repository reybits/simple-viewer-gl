/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Common/Buffer.h"
#include "Types/Types.h"

#include <string>

namespace helpers
{
    enum class Platform
    {
        Unknown,
        Win32,
        Cocoa,
        Wayland,
        X11,
    };

    Platform getPlatform();

    uint16_t read_uint16(const uint8_t* p);
    uint32_t read_uint32(const uint8_t* p);
    void swap_uint32s(uint8_t* p, uint32_t size);
    void swap_uint16s(uint8_t* p, uint32_t size);

    void trimRightSpaces(char* buf);

    uint32_t nextPot(uint32_t n);
    uint32_t calculatePitch(uint32_t width, uint32_t bitsPerPixel);

    uint64_t getTime();

    void replaceAll(std::string& subject, const std::string& search, const std::string& replace);

    char* memfind(const char* buf, size_t size, const char* tofind);

    bool base64decode(const char* input, size_t in_len, Buffer& out);

    std::string getDirectoryFromPath(const char* path);

} // namespace helpers

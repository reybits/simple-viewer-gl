/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "helpers.h"

#include <GLFW/glfw3.h>

#include <cmath>
#include <cstring>
#include <ctime>
#include <string>

namespace helpers
{
    Platform getPlatform()
    {
#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 4
        auto platform = glfwGetPlatform();
        switch (platform)
        {
        case GLFW_PLATFORM_WIN32:
            return Platform::Win32;

        case GLFW_PLATFORM_COCOA:
            return Platform::Cocoa;

        case GLFW_PLATFORM_WAYLAND:
            return Platform::Wayland;

        case GLFW_PLATFORM_X11:
            return Platform::X11;
        }
#endif

        return Platform::Unknown;
    }

    uint16_t read_uint16(const uint8_t* p)
    {
        return ((uint16_t)p[0] << 8) | p[1];
    }

    uint32_t read_uint32(const uint8_t* p)
    {
        return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
    }

    void swap_uint32s(uint8_t* p, uint32_t size)
    {
        const uint32_t items = size / 4;
        for (uint32_t i = 0; i < items; i++)
        {
            *((uint32_t*)p) = read_uint32(p);
            p += 4;
        }
    }

    void swap_uint16s(uint8_t* p, uint32_t size)
    {
        const uint32_t items = size / 2;
        for (uint32_t i = 0; i < items; i++)
        {
            *((uint16_t*)p) = read_uint16(p);
            p += 2;
        }
    }

    void trimRightSpaces(char* buf)
    {
        char* s = buf - 1;
        for (; *buf; ++buf)
        {
            if (*buf != ' ')
            {
                s = buf;
            }
        }
        *++s = 0; // nul terminate the string on the first of the final spaces
    }

    uint32_t nextPot(uint32_t n)
    {
        if (n <= 1)
        {
            return 1;
        }

        n--;
        n |= n >> 1;
        n |= n >> 2;
        n |= n >> 4;
        n |= n >> 8;
        n |= n >> 16;
        return n + 1;
    }

    uint32_t calculatePitch(uint32_t width, uint32_t bitsPerPixel)
    {
        // texture pitch should be multiple by 4
        auto bytesPerPixel = (bitsPerPixel + 7) / 8;
        auto rawPitch = width * bytesPerPixel;
        return (rawPitch + 3) & ~3u;
    }

    uint64_t getTime()
    {
        timespec now;
        clock_gettime(CLOCK_REALTIME, &now);
        return now.tv_sec * 1000000000 + now.tv_nsec;
    }

    void replaceAll(std::string& subject, const std::string& search, const std::string& replace)
    {
        const size_t searchLen = search.length();

        size_t index = 0;
        while (true)
        {
            index = subject.find(search, index);
            if (index == std::string::npos)
            {
                break;
            }

            subject.replace(index, searchLen, replace);
            index += replace.length();
        }
    }

    char* memfind(const char* buf, size_t size, const char* tofind)
    {
        const auto findlen = ::strlen(tofind);
        if (findlen > size)
        {
            return nullptr;
        }

        if (size < 1)
        {
            return (char*)buf;
        }

        {
            const char* bufend = &buf[size - findlen + 1];
            const char* c = buf;
            for (; c < bufend; c++)
            {
                if (*c == *tofind)
                { // first letter matches
                    if (::memcmp(c + 1, tofind + 1, findlen - 1) == 0)
                    { // found
                        return (char*)c;
                    }
                }
            }
        }

        return nullptr;
    }

    bool base64decode(const char* input, size_t in_len, Buffer& out)
    {
        static constexpr uint8_t LookupTable[] = {
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
            52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
            64, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
            15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
            64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
            41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
            64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
        };

        if (in_len % 4 != 0)
        {
            return false; // "Input data size is not a multiple of 4"
        }

        size_t out_len = in_len / 4 * 3;
        if (input[in_len - 1] == '=')
        {
            out_len--;
        }
        if (input[in_len - 2] == '=')
        {
            out_len--;
        }

        out.resize(out_len);

        for (size_t i = 0, j = 0; i < in_len;)
        {
            uint32_t a = input[i] == '=' ? 0 & i++ : LookupTable[(int)input[i++]];
            uint32_t b = input[i] == '=' ? 0 & i++ : LookupTable[(int)input[i++]];
            uint32_t c = input[i] == '=' ? 0 & i++ : LookupTable[(int)input[i++]];
            uint32_t d = input[i] == '=' ? 0 & i++ : LookupTable[(int)input[i++]];

            uint32_t triple = (a << 3 * 6) + (b << 2 * 6) + (c << 1 * 6) + (d << 0 * 6);

            if (j < out_len)
            {
                out[j++] = (triple >> 2 * 8) & 0xFF;
            }
            if (j < out_len)
            {
                out[j++] = (triple >> 1 * 8) & 0xFF;
            }
            if (j < out_len)
            {
                out[j++] = (triple >> 0 * 8) & 0xFF;
            }
        }

        return true;
    }

    std::string getDirectoryFromPath(const char* path)
    {
        if (path != nullptr)
        {
            std::string fullPath = path;
            auto pos = fullPath.find_last_of("/\\");
            if (pos != std::string::npos)
            {
                return fullPath.substr(0, pos);
            }
        }

        return ".";
    }

} // namespace helpers

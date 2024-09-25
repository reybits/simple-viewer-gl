/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include "buffer.h"

#include <GLFW/glfw3.h>
#include <string>

struct sBitmapDescription
{
    void reset()
    {
        bitmap.clear();
        format      = GL_RGB;
        bpp         = 0;
        pitch       = 0;
        width       = 0;
        height      = 0;

        bppImage    = 0;
        size        = -1;

        images      = 0;
        current     = 0;

        isAnimation = false;
        delay       = 0;

        exifList.clear();
    }

    // buffer related
    Buffer bitmap;
    GLenum format     = GL_RGB;
    uint32_t bpp      = 0;
    uint32_t pitch    = 0;
    uint32_t width    = 0;
    uint32_t height   = 0;

    // file related
    uint32_t bppImage = 0;  // bit per pixel of original image
    long size         = -1; // file size on disk

    uint32_t images   = 0;
    uint32_t current  = 0;

    bool isAnimation  = false;
    uint32_t delay    = 0; // frame animation delay

    struct ExifEntry
    {
        std::string tag;
        std::string value;
    };
    typedef std::vector<ExifEntry> ExifList;
    ExifList exifList;
};

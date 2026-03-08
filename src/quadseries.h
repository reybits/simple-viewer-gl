/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include "quad.h"

class cQuadSeries : public cQuad
{
public:
    cQuadSeries(uint32_t tw, uint32_t th, const uint8_t* data, ePixelFormat bitmapFormat);
    virtual ~cQuadSeries();

    void setup(uint32_t width, uint32_t height, uint32_t count);
    void setFrame(uint32_t frame);

private:
    uint32_t m_count;
    uint32_t m_frame;
};

/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "QuadSeries.h"

cQuadSeries::cQuadSeries(uint32_t tw, uint32_t th, const uint8_t* data, ePixelFormat bitmapFormat)
    : cQuad(tw, th, data, bitmapFormat)
    , m_count(1)
    , m_frame(0)
{
}

cQuadSeries::~cQuadSeries()
{
}

void cQuadSeries::setup(uint32_t width, uint32_t height, uint32_t count)
{
    m_size = { (float)width, (float)height };
    m_count = count;
}

void cQuadSeries::setFrame(uint32_t frame)
{
    m_frame = frame;
    m_frame %= m_count;

    const float w = m_size.x;
    const float h = m_size.y;

    uint32_t cols = (uint32_t)(m_tw / w);

    float tx1 = m_frame * w;
    float ty1 = 0;

    if (tx1 > m_tw - w && cols > 0)
    {
        m_frame -= (int)(m_tw / w);
        tx1 = w * (m_frame % cols);
        ty1 += h * (1 + m_frame / cols);
    }

    float tx2 = (tx1 + w) / m_tw;
    float ty2 = (ty1 + h) / m_th;

    tx1 /= m_tw;
    ty1 /= m_th;

    m_quad.v[0].tx = tx1;
    m_quad.v[0].ty = ty1;
    m_quad.v[1].tx = tx2;
    m_quad.v[1].ty = ty1;
    m_quad.v[2].tx = tx2;
    m_quad.v[2].ty = ty2;
    m_quad.v[3].tx = tx1;
    m_quad.v[3].ty = ty2;
}

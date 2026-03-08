/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
*  Texture filtration
*  by Timo Suoranta <tksuoran@gmail.com>
*
\**********************************************/

#include "quad.h"

#include <cmath>
#include <cstdio>

cQuad::cQuad(uint32_t tw, uint32_t th, const uint8_t* data, GLenum bitmapFormat)
    : m_tw(tw)
    , m_th(th)
    , m_format(bitmapFormat)
    , m_size(tw, th)
    , m_filter(true)
{
    if (data != nullptr)
    {
        m_quad.tex = render::createTexture();
    }
    setData(data);

    // by deafult set whole texture size
    setSpriteSize({ (float)tw, (float)th });
}

cQuad::cQuad(uint32_t tw, uint32_t th, const uint8_t* data, GLenum internalFormat, uint32_t dataSize)
    : m_tw(tw)
    , m_th(th)
    , m_format(internalFormat)
    , m_size(tw, th)
    , m_filter(true)
{
    m_quad.tex = render::createTexture();
    render::setCompressedData(m_quad.tex, data, tw, th, internalFormat, dataSize);

    setSpriteSize({ (float)tw, (float)th });
}

cQuad::~cQuad()
{
    if (m_quad.tex != 0)
    {
        render::deleteTexture(m_quad.tex);
    }
}

const Quad& cQuad::getQuad() const
{
    return m_quad;
}

void cQuad::setData(const uint8_t* data)
{
    m_filter = true;
    if (data != nullptr && m_quad.tex == 0)
    {
        m_quad.tex = render::createTexture();
    }
    render::setData(m_quad.tex, data, m_tw, m_th, m_format);
}

void cQuad::setColor(const cColor& color)
{
    render::setColor(&m_quad, color);
}

void cQuad::setTextureRect(const Vectorf& pos, const Vectorf& size)
{
    m_size = size;

    const float invTw = m_tw ? (1.0f / m_tw) : 0.0f;
    const float invTh = m_th ? (1.0f / m_th) : 0.0f;

    const float tx1 = pos.x * invTw;
    const float ty1 = pos.y * invTh;
    const float tx2 = (pos.x + size.x) * invTw;
    const float ty2 = (pos.y + size.y) * invTh;

    m_quad.v[0].tx = tx1;
    m_quad.v[0].ty = ty1;

    m_quad.v[1].tx = tx2;
    m_quad.v[1].ty = ty1;

    m_quad.v[2].tx = tx2;
    m_quad.v[2].ty = ty2;

    m_quad.v[3].tx = tx1;
    m_quad.v[3].ty = ty2;
}

void cQuad::setSpriteSize(const Vectorf& size)
{
    setTextureRect({ 0.0f, 0.0f }, size);
}

void cQuad::render(const Vectorf& pos)
{
    renderEx(pos, m_size);
}

void cQuad::renderEx(const Vectorf& pos, const Vectorf& size, int angle)
{
    if (angle == 0)
    {
        m_quad.v[0].x = pos.x;
        m_quad.v[0].y = pos.y;
        m_quad.v[1].x = pos.x + size.x;
        m_quad.v[1].y = pos.y;
        m_quad.v[2].x = pos.x + size.x;
        m_quad.v[2].y = pos.y + size.y;
        m_quad.v[3].x = pos.x;
        m_quad.v[3].y = pos.y + size.y;
    }
    else
    {
        const float a = M_PI * angle / 180.0f;
        const float c = ::cosf(a);
        const float s = ::sinf(a);

        m_quad.v[0].x = pos.x;
        m_quad.v[0].y = pos.y;
        m_quad.v[1].x = pos.x + size.x * c;
        m_quad.v[1].y = pos.y + size.x * s;
        m_quad.v[2].x = pos.x + size.x * c - size.y * s;
        m_quad.v[2].y = pos.y + size.x * s + size.y * c;
        m_quad.v[3].x = pos.x - size.y * s;
        m_quad.v[3].y = pos.y + size.y * c;
    }

    render::render(m_quad);
}

void cQuad::useFilter(bool filter)
{
    if (m_filter != filter)
    {
        m_filter = filter;

        render::bindTexture(m_quad.tex);
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter ? GL_LINEAR : GL_NEAREST));
        // glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter ? GL_LINEAR : GL_NEAREST);
    }
}

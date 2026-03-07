/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include "renderer.h"

class cQuad
{
public:
    cQuad(uint32_t tw, uint32_t th, const uint8_t* data = 0, GLenum bitmapFormat = GL_RGB);
    cQuad(uint32_t tw, uint32_t th, const uint8_t* data, GLenum internalFormat, uint32_t dataSize);
    virtual ~cQuad();

    virtual void setData(const uint8_t* data);
    virtual void setColor(const cColor& color);
    virtual void setTextureRect(const Vectorf& pos, const Vectorf& size);
    virtual void setSpriteSize(const Vectorf& size);
    virtual void render(const Vectorf& pos);
    virtual void renderEx(const Vectorf& pos, const Vectorf& size, int rot = 0);

    const sQuad& getQuad() const;

    virtual uint32_t getTexWidth() const
    {
        return m_tw;
    }
    virtual uint32_t getTexHeight() const
    {
        return m_th;
    }

    virtual const Vectorf& getSize() const
    {
        return m_size;
    }

    virtual void useFilter(bool filter);

    GLenum getFormat() const
    {
        return m_format;
    }

protected:
    // texture size
    uint32_t m_tw;
    uint32_t m_th;
    GLenum m_format;

    // sprite size
    Vectorf m_size;

    bool m_filter;

    sQuad m_quad;
};

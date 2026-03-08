/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "ImageBorder.h"

cImageBorder::cImageBorder()
{
    setColor({ 25, 255, 25, 255 });
}

cImageBorder::~cImageBorder()
{
}

void cImageBorder::setColor(const cColor& color)
{
    m_quad.v[0].color = m_quad.v[1].color = m_quad.v[2].color = m_quad.v[3].color = color;
}

void cImageBorder::render(float x, float y, float w, float h)
{
    const float delta = getThickness() / render::getZoom();

    renderLine(x - delta, y - delta, w + delta * 2, delta); // up
    renderLine(x - delta,     y + h, w + delta * 2, delta); // down
    renderLine(x - delta,         y,         delta,     h); // left
    renderLine(    x + w,         y,         delta,     h); // right
}

void cImageBorder::renderLine(float x, float y, float w, float h)
{
    m_quad.v[0].x = x;
    m_quad.v[0].y = y;
    m_quad.v[1].x = x + w;
    m_quad.v[1].y = y;
    m_quad.v[2].x = x + w;
    m_quad.v[2].y = y + h;
    m_quad.v[3].x = x;
    m_quad.v[3].y = y + h;
    render::bindTexture(0);
    render::render(m_quad);
}

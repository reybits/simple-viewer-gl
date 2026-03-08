/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
*  Image Grid
*  by Timo Suoranta <tksuoran@gmail.com>
*
\**********************************************/

#include "imagegrid.h"
#include "types/math.h"

cImageGrid::cImageGrid()
{
    setColor(cColor::Black);
}

cImageGrid::~cImageGrid()
{
}

void cImageGrid::setColor(const cColor& color)
{
    m_line.v[0].color = m_line.v[1].color = color;
}

void cImageGrid::render(float x, float y, float w, float h)
{
    float zoom = render::getZoom();
    float alpha = zoom / 16.0f;
    int alpha_i = clamp(0, 255, static_cast<int>(alpha * 255.0f));
    if ((alpha_i == 0) || (zoom < 2.0f))
    {
        return;
    }

    render::bindTexture(0);

    m_line.v[0].color.a = m_line.v[1].color.a = alpha_i;

    m_line.v[0].x = x;
    m_line.v[1].x = x + w;
    for (float i = y; i <= y + h; i += 1.0f)
    {
        m_line.v[0].y = m_line.v[1].y = i;
        render::render(m_line);
    }

    m_line.v[0].y = y;
    m_line.v[1].y = y + h;
    for (float i = x; i <= x + w; i += 1.0f)
    {
        m_line.v[0].x = m_line.v[1].x = i;
        render::render(m_line);
    }
}

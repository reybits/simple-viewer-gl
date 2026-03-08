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

#include <algorithm>
#include <vector>

cImageGrid::cImageGrid()
{
    setColor(cColor::Black);
}

cImageGrid::~cImageGrid()
{
}

void cImageGrid::setColor(const cColor& color)
{
    m_color = color;
}

void cImageGrid::render(float x, float y, float w, float h)
{
    const float zoom = render::getZoom();
    const float alpha = zoom / 16.0f;
    const int alpha_i = std::clamp(static_cast<int>(alpha * 255.0f), 0, 255);
    if ((alpha_i == 0) || (zoom < 2.0f))
    {
        return;
    }

    cColor color = m_color;
    color.a = static_cast<uint8_t>(alpha_i);

    std::vector<Vertex> vertices;
    const auto hLines = static_cast<uint32_t>(h) + 1;
    const auto vLines = static_cast<uint32_t>(w) + 1;
    vertices.reserve((hLines + vLines) * 2);

    for (float i = y; i <= y + h; i += 1.0f)
    {
        vertices.push_back({ x, i, 0, 0, color });
        vertices.push_back({ x + w, i, 0, 0, color });
    }

    for (float i = x; i <= x + w; i += 1.0f)
    {
        vertices.push_back({ i, y, 0, 0, color });
        vertices.push_back({ i, y + h, 0, 0, color });
    }

    if (!vertices.empty())
    {
        render::renderLines(vertices.data(), static_cast<uint32_t>(vertices.size()));
    }
}

/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "checkerboard.h"
#include "common/config.h"
#include "common/helpers.h"

#include <cmath>
#include <vector>

cCheckerboard::cCheckerboard(const sConfig& config)
    : m_config(config)
{
}

void cCheckerboard::init()
{
    const uint32_t cellSize = helpers::nextPot(m_config.bgCellSize);
    const uint32_t texSize = cellSize * 2;

    std::vector<uint8_t> buffer(texSize * texSize);
    auto p = buffer.data();

    constexpr uint8_t Colors[2] = { 0xc8, 0x7d };

    for (uint32_t y = 0; y < texSize; y++)
    {
        for (uint32_t x = 0; x < texSize; x++)
        {
            const auto color = (y / cellSize + x / cellSize) % 2 == 0
                ? Colors[0]
                : Colors[1];
            *p++ = color;
        }
    }

    m_cb.reset(new cQuad(texSize, texSize, buffer.data(), GL_LUMINANCE));
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    m_cb->useFilter(false);
}

void cCheckerboard::render()
{
    if (m_config.backgroundIndex == 0)
    {
        auto& viewport = cRenderer::getViewportSize();
        Vectorf pos{ -(float)(viewport.x >> 1),
                     -(float)(viewport.y >> 1) };
        Vectorf size{ (float)viewport.x,
                      (float)viewport.y };

        m_cb->setTextureRect(pos, size);
        m_cb->render({ 0.0f, 0.0f });
    }
    else
    {
        if (m_config.backgroundIndex == 1)
        {
            auto c = m_config.bgColor.toGL();
            glClearColor(c.r, c.g, c.b, c.a);
        }
        else if (m_config.backgroundIndex == 2)
        {
            glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        }
        else if (m_config.backgroundIndex == 3)
        {
            glClearColor(0.0f, 1.0f, 0.0f, 1.0f);
        }
        else // if (m_config.backgroundIndex == 4)
        {
            glClearColor(0.0f, 0.0f, 1.0f, 1.0f);
        }

        glClear(GL_COLOR_BUFFER_BIT);
    }
}

/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "selection.h"
#include "quad.h"
#include "types/math.h"
#include "types/vector.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace
{

    const float delta = 20.0f;
    const float delta2 = delta * 0.5f;

    const uint32_t m_cellSize = 8;
    const uint32_t m_texSize = m_cellSize * 2;

} // namespace

void cSelection::init()
{
    std::vector<uint8_t> buffer(m_cellSize * m_texSize);
    const uint8_t colors[2] = { 0x30, 0xe0 };

    {
        auto p = buffer.data();
        uint32_t idx = 0;

        for (uint32_t y = 0; y < m_texSize; y++)
        {
            if (y % m_cellSize == 0)
            {
                idx = (idx + 1) % 2;
            }

            for (uint32_t x = 0; x < m_cellSize; x++)
            {
                const auto color = colors[idx];
                *p++ = color;
            }
        }

        m_vert.reset(new cQuad(m_cellSize, m_texSize, buffer.data(), GL_LUMINANCE));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
        m_vert->useFilter(false);
    }

    {
        auto p = buffer.data();
        uint32_t idx = 1;

        for (uint32_t y = 0; y < m_cellSize; y++)
        {
            for (uint32_t x = 0; x < m_texSize; x++)
            {
                if (x % m_cellSize == 0)
                {
                    idx = (idx + 1) % 2;
                }

                const auto color = colors[idx];
                *p++ = color;
            }
        }

        m_hori.reset(new cQuad(m_texSize, m_cellSize, buffer.data(), GL_LUMINANCE));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
        m_hori->useFilter(false);
    }
}

void cSelection::setImageDimension(int w, int h)
{
    m_imageWidth = w;
    m_imageHeight = h;
    m_rc.clear();
    m_rcTest.clear();
}

void cSelection::updateTestRect(float scale)
{
    if (m_rc.isSet())
    {
        m_rc.normalize();
        auto& rc = m_rc;
        const Vectorf d{ delta2 / scale, delta2 / scale };
        m_rcTest.set(rc.tl - d, rc.br + d);
    }
}

void cSelection::mouseButton(const Vectorf& p, float scale, bool pressed)
{
    if (pressed)
    {
        updateTestRect(scale);

        const Vectorf pos{ ::roundf(p.x), ::roundf(p.y) };
        const bool inside = m_rcTest.testPoint(pos);

        m_mousePos = pos;

        if (m_mode == eMouseMode::None)
        {
            if (inside)
            {
                m_mode = (m_corner == (uint32_t)Edge::Center ? eMouseMode::Move : eMouseMode::Resize);
            }
            else
            {
                m_mode = eMouseMode::Select;
                m_rc.setLeftTop(pos);
                m_rc.clear();
                const Vectorf d{ delta2 / scale, delta2 / scale };
                m_rcTest.setLeftTop(pos - d);
                m_rcTest.clear();
            }
        }
        else if (inside == false)
        {
            m_mode = eMouseMode::None;
            m_rc.clear();
            m_rcTest.clear();
        }
    }
    else
    {
        if (m_rc.width() == 0 || m_rc.height() == 0)
        {
            m_rc.clear();
            m_rcTest.clear();
        }
        m_mode = eMouseMode::None;
    }
}

void cSelection::mouseMove(const Vectorf& p, float scale)
{
    const Vectorf pos{ ::roundf(p.x), ::roundf(p.y) };

    if (m_mode == eMouseMode::None)
    {
        updateCorner(pos, scale);
    }
    else
    {
        Vectorf delta = pos - m_mousePos;
        m_mousePos = pos;

        switch (m_mode)
        {
        case eMouseMode::None: // do nothing here
            break;

        case eMouseMode::Select:
            m_rc.setRightBottom(pos);
            break;

        case eMouseMode::Move:
            clampShiftDelta(delta);
            m_rc.shiftRect(delta);
            break;

        case eMouseMode::Resize:
            if ((m_corner & (uint32_t)Edge::Top))
            {
                m_rc.tl.y += delta.y;
            }
            if ((m_corner & (uint32_t)Edge::Right))
            {
                m_rc.br.x += delta.x;
            }
            if ((m_corner & (uint32_t)Edge::Bottom))
            {
                m_rc.br.y += delta.y;
            }
            if ((m_corner & (uint32_t)Edge::Left))
            {
                m_rc.tl.x += delta.x;
            }
            break;
        }

        m_rc.tl.x = ::roundf(clamp<float>(0.0f, m_imageWidth, m_rc.tl.x));
        m_rc.tl.y = ::roundf(clamp<float>(0.0f, m_imageHeight, m_rc.tl.y));
        m_rc.br.x = ::roundf(clamp<float>(0.0f, m_imageWidth, m_rc.br.x));
        m_rc.br.y = ::roundf(clamp<float>(0.0f, m_imageHeight, m_rc.br.y));
    }
}

void cSelection::clampShiftDelta(Vectorf& delta)
{
    delta.x = ::roundf(clamp<float>(-m_rc.tl.x, m_imageWidth - m_rc.br.x, delta.x));
    delta.y = ::roundf(clamp<float>(-m_rc.tl.y, m_imageHeight - m_rc.br.y, delta.y));
}

void cSelection::render(const Vectorf& offset)
{
    if (m_rc.isSet())
    {
        Rectf rc;
        setImagePos(rc, offset);

        const float x = rc.tl.x;
        const float y = rc.tl.y;
        const float w = rc.width();
        const float h = rc.height();

        const float thickness = 4.0f;
        const float d = thickness / cRenderer::getZoom();

#if 0
        const Vectori delta{ delta2, delta2 };
        if (m_corner & (uint32_t)Edge::Top)
        {
            if (m_corner & (uint32_t)Edge::Right)
            {
                const Vectori pos{ rc.br.x, rc.tl.y - delta2 };
                renderRect(pos, pos + delta, d);
            }
            else if (m_corner & (uint32_t)Edge::Left)
            {
                renderRect(rc.tl - delta, rc.tl, d);
            }
            else
            {
                const Vectori pos{ rc.tl.x, rc.tl.y - delta2 };
                renderRect(pos, { rc.br.x, rc.tl.y }, d);
            }
        }
        else if (m_corner & (uint32_t)Edge::Bottom)
        {
            if (m_corner & (uint32_t)Edge::Right)
            {
                renderRect(rc.br, rc.br + delta, d);
            }
            else if (m_corner & (uint32_t)Edge::Left)
            {
                const Vectori pos{ rc.tl.x - delta2, rc.br.y };
                renderRect(pos, { rc.tl.x, rc.br.y + delta2 }, d);
            }
            else
            {
                const Vectori pos{ rc.br.x, rc.br.y + delta2 };
                renderRect({ rc.tl.x, rc.br.y }, pos, d);
            }
        }
        else if (m_corner & (uint32_t)Edge::Left)
        {
            renderRect({ rc.tl.x - delta2, rc.tl.y }, { rc.tl.x, rc.br.y }, d);
        }
        else if (m_corner & (uint32_t)Edge::Right)
        {
            renderRect({ rc.br.x, rc.tl.y }, { rc.br.x + delta2, rc.br.y }, d);
        }
#endif

        // top line
        setColor(m_corner & (uint32_t)Edge::Top);
        renderHorizontal({ x - d, y - d }, w + d * 2.0f, d);
        // bottom line
        setColor(m_corner & (uint32_t)Edge::Bottom);
        renderHorizontal({ x - d, y + h }, w + d * 2.0f, d);
        // left line
        setColor(m_corner & (uint32_t)Edge::Left);
        renderVertical({ x - d, y }, h, d);
        // right line
        setColor(m_corner & (uint32_t)Edge::Right);
        renderVertical({ x + w, y }, h, d);
    }
}

const Rectf& cSelection::getRect() const
{
    return m_rc;
}

int cSelection::getCursor() const
{
    static const int cursor[16 + 1] = {
        //                                  1  1  1  1  1
        // 0  1  2  3  4  5  6  7  8  9  0  1  2  3  4  5
        0, 3, 2, 5, 3, 0, 4, 2, 2, 4, 0, 3, 5, 2, 3, 1, 1
    };
    // printf("corner: %u , cursor: %d\n", m_corner, cursor[m_corner]);
    return cursor[m_corner];
}

void cSelection::updateCorner(const Vectorf& pos, float scale)
{
    updateTestRect(scale);
    const auto& rc = m_rcTest;
    if (rc.testPoint(pos))
    {
        m_corner = 0;

        const float d = delta / scale;

        const Rectf rcLt(rc.tl, { rc.tl.x + d, rc.br.y });
        if (rcLt.testPoint(pos))
        {
            m_corner |= (uint32_t)Edge::Left;
        }

        const Rectf rcRt({ rc.br.x - d, rc.tl.y }, rc.br);
        if (rcRt.testPoint(pos))
        {
            m_corner |= (uint32_t)Edge::Right;
        }

        const Rectf rcUp(rc.tl, { rc.br.x, rc.tl.y + d });
        if (rcUp.testPoint(pos))
        {
            m_corner |= (uint32_t)Edge::Top;
        }

        const Rectf rcDn({ rc.tl.x, rc.br.y - d }, rc.br);
        if (rcDn.testPoint(pos))
        {
            m_corner |= (uint32_t)Edge::Bottom;
        }

        if (!m_corner)
        {
            m_corner = (uint32_t)Edge::Center;
        }
    }
    else
    {
        m_corner = (uint32_t)Edge::None;
    }
}

void cSelection::renderHorizontal(const Vectorf& pos, float w, float thickness)
{
    const float offset = glfwGetTime() * 30.0f;
    m_hori->setTextureRect({ offset, 0.0f }, { w * cRenderer::getZoom(), thickness });
    m_hori->renderEx(pos, { w, thickness });
}

void cSelection::renderVertical(const Vectorf& pos, float h, float thickness)
{
    const float offset = glfwGetTime() * 30.0f;
    m_vert->setTextureRect({ 0.0f, offset }, { thickness, h * cRenderer::getZoom() });
    m_vert->renderEx(pos, { thickness, h });
}

#if 0
void cSelection::renderRect(const Vectorf& tl, const Vectorf& br, float thickness)
{
    setColor(true);
    const auto d = thickness;
    const auto w = br.x - tl.x;
    const auto h = br.y - tl.y;
    renderHorizontal(tl, w, d);
    renderHorizontal({ tl.x, br.y }, w, d);
    renderVertical(tl, h, d);
    renderVertical({ br.x, tl.y }, h, d);
}
#endif

void cSelection::setImagePos(Rectf& rc, const Vectorf& offset)
{
    rc = m_rc;
    rc.shiftRect(offset);
    rc.normalize();
}

void cSelection::setColor(bool selected)
{
    m_hori->setColor(selected == false ? cColor::White : cColor::Green);
    m_vert->setColor(selected == false ? cColor::White : cColor::Green);
}

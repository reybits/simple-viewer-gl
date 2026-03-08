/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Types/Rect.h"
#include "Types/Vector.h"

#include <cstdint>
#include <memory>

class cQuad;

class cSelection final
{
public:
    void init();
    void setImageDimension(int w, int h);
    void mouseButton(const Vectorf& pos, float scale, bool pressed);
    void mouseMove(const Vectorf& pos, float scale);
    void render(const Vectorf& offset);
    const Rectf& getRect() const;
    int getCursor() const;

private:
    void updateTestRect(float scale);
    void updateCorner(const Vectorf& pos, float scale);
    void renderHorizontal(const Vectorf& pos, float w, float thickness);
    void renderVertical(const Vectorf& pos, float h, float thickness);
#if 0
    void renderRect(const Vectorf& tl, const Vectorf& br, float thickness);
#endif
    void setImagePos(Rectf& rc, const Vectorf& offset);
    void setColor(bool selected);
    void clampShiftDelta(Vectorf& delta);

private:
    int m_imageWidth = 0;
    int m_imageHeight = 0;
    Vectorf m_mousePos = { 0.0f, 0.0f };

    enum class eMouseMode
    {
        None,
        Select,
        Move,
        Resize
    };
    eMouseMode m_mode = eMouseMode::None;

    enum Edge : uint32_t
    {
        EdgeNone = 0,
        EdgeTop = 1 << 0,
        EdgeRight = 1 << 1,
        EdgeBottom = 1 << 2,
        EdgeLeft = 1 << 3,
        EdgeCenter = 1 << 4,
    };
    uint32_t m_corner = EdgeNone;

    std::unique_ptr<cQuad> m_hori;
    std::unique_ptr<cQuad> m_vert;
    Rectf m_rc;
    Rectf m_rcTest;
};

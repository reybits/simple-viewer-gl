/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Renderer.h"

class cImageBorder final
{
public:
    cImageBorder();
    ~cImageBorder();

    void setColor(const cColor& color);
    void render(float x, float y, float w, float h);

    void setThickness(float thickness)
    {
        m_thickness = thickness;
    }

    float getThickness() const
    {
        return m_thickness;
    }

private:
    void renderLine(float x, float y, float w, float h);

private:
    Quad m_quad;
    float m_thickness = 2.0f;
};

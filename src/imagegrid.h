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

#pragma once

#include "renderer.h"

class cImageGrid final
{
public:
    cImageGrid();
    ~cImageGrid();

    void setColor(const cColor& color);
    void render(float x, float y, float w, float h);

private:
    Line m_line;
};

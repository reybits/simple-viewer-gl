/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
*  Image Grid
*  by Timo Suoranta <tksuoran@gmail.com>
*
\**********************************************/

#pragma once

#include "Types/Color.h"

class cImageGrid final
{
public:
    cImageGrid();
    ~cImageGrid();

    void setColor(const cColor& color);
    void render(float x, float y, float w, float h);

private:
    cColor m_color;
};

/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include <cstdint>

class cColor final
{
public:
    cColor() = default;

    cColor(uint8_t R, uint8_t G, uint8_t B, uint8_t A)
        : r(R)
        , g(G)
        , b(B)
        , a(A)
    {
    }

    static const cColor Transparent;
    static const cColor Black;
    static const cColor White;
    static const cColor Red;
    static const cColor Green;
    static const cColor Blue;
    static const cColor Yellow;

    struct GLcolor
    {
        float r;
        float g;
        float b;
        float a;
    };

    GLcolor toGL() const
    {
        return {
            r / 255.0f,
            g / 255.0f,
            b / 255.0f,
            a / 255.0f,
        };
    }

public:
    uint8_t r = 255;
    uint8_t g = 255;
    uint8_t b = 255;
    uint8_t a = 255;
};

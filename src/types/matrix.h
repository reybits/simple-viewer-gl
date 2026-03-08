/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include <cmath>
#include <cstring>

struct Matrix4
{
    float m[16];

    static Matrix4 Identity()
    {
        Matrix4 r;
        std::memset(r.m, 0, sizeof(r.m));
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    static Matrix4 Ortho(float l, float r, float b, float t, float n, float f)
    {
        Matrix4 o;
        std::memset(o.m, 0, sizeof(o.m));
        o.m[0] = 2.0f / (r - l);
        o.m[5] = 2.0f / (t - b);
        o.m[10] = -2.0f / (f - n);
        o.m[12] = -(r + l) / (r - l);
        o.m[13] = -(t + b) / (t - b);
        o.m[14] = -(f + n) / (f - n);
        o.m[15] = 1.0f;
        return o;
    }

    static Matrix4 RotateZ(float degrees)
    {
        const float rad = degrees * (3.14159265358979323846f / 180.0f);
        const float c = std::cos(rad);
        const float s = std::sin(rad);
        Matrix4 r = Identity();
        r.m[0] = c;
        r.m[1] = s;
        r.m[4] = -s;
        r.m[5] = c;
        return r;
    }

    Matrix4 operator*(const Matrix4& b) const
    {
        Matrix4 r;
        for (int col = 0; col < 4; col++)
        {
            for (int row = 0; row < 4; row++)
            {
                r.m[col * 4 + row] =
                    m[0 * 4 + row] * b.m[col * 4 + 0] +
                    m[1 * 4 + row] * b.m[col * 4 + 1] +
                    m[2 * 4 + row] * b.m[col * 4 + 2] +
                    m[3 * 4 + row] * b.m[col * 4 + 3];
            }
        }
        return r;
    }
};

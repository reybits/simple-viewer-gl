/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "vector.h"

#include <cmath>

template<typename T>
class cRect final
{
public:
    cRect()
        : m_isSet(false)
    { }

    cRect(const cVector<T>& topLeft, const cVector<T>& bottomRight)
        : m_isSet(true)
        , tl(topLeft)
        , br(bottomRight)
    { }

    void clear()
    {
        m_isSet = false;
    }

    bool isSet() const
    {
        return m_isSet;
    }

    void set(const cVector<T>& topLeft, const cVector<T>& bottomRight)
    {
        setLeftTop(topLeft);
        setRightBottom(bottomRight);
    }

    void setLeftTop(const cVector<T>& topLeft)
    {
        m_isSet = true;
        tl = topLeft;
    }

    void setRightBottom(const cVector<T>& bottomRight)
    {
        m_isSet = true;
        br = bottomRight;
    }

    void shiftRect(const cVector<T>& delta)
    {
        tl += delta;
        br += delta;
    }

    void encapsulate(const cVector<T>& v)
    {
        if (m_isSet)
        {
            tl.x = v.x < tl.x ? v.x : tl.x;
            tl.y = v.y < tl.y ? v.y : tl.y;
            br.x = v.x > br.x ? v.x : br.x;
            br.y = v.y > br.y ? v.y : br.y;
        }
        else
        {
            m_isSet = true;
            tl = v;
            br = v;
        }
    }

    bool testPoint(const cVector<T>& v) const
    {
        return !(!m_isSet || v.x < tl.x || v.x >= br.x || v.y < tl.y || v.y >= br.y);
    }

    bool intersect(const cRect<T>& rc) const
    {
        if (fabs(tl.x + br.x - rc.tl.x - rc.br.x) < (br.x - tl.x + rc.br.x - rc.tl.x) &&
            fabs(tl.y + br.y - rc.tl.y - rc.br.y) < (br.y - tl.y + rc.br.y - rc.tl.y))
        {
            return true;
        }
        return false;
    }

    T width() const
    {
        return (br.x >= tl.x) ? (br.x - tl.x) : (tl.x - br.x);
    }

    T height() const
    {
        return (br.y >= tl.y) ? (br.y - tl.y) : (tl.y - br.y);
    }

    void normalize()
    {
        if (tl.x > br.x)
        {
            auto x = br.x;
            br.x = tl.x;
            tl.x = x;
        }
        if (tl.y > br.y)
        {
            auto y = br.y;
            br.y = tl.y;
            tl.y = y;
        }
    }

    cRect& operator=(const cRect<T>& rc)
    {
        if (&rc != this)
        {
            m_isSet = rc.m_isSet;
            tl.x = rc.tl.x;
            br.x = rc.br.x;
            tl.y = rc.tl.y;
            br.y = rc.br.y;
        }
        return *this;
    }

private:
    bool m_isSet;

public:
    cVector<T> tl;
    cVector<T> br;
};

typedef cRect<float> Rectf;
typedef cRect<int> Recti;

/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

class cScale
{
public:
    void setScalePercent(int scale)
    {
        m_percent = scale;
        m_scale = scale / 100.0f;
    }

    int getScalePercent() const
    {
        return m_percent;
    }

    void setScale(float scale)
    {
        m_percent = (int)(scale * 100.0f);
        m_scale = scale;
    }

    float getScale() const
    {
        return m_scale;
    }

private:
    int m_percent = 100;
    float m_scale = 1.0f;
};

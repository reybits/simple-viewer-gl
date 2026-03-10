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
#include <string>

class cProgress final
{
public:
    cProgress() = default;

    void show();
    void hide();

    void setStatus(const std::string& text);
    void setPercent(float progress);
    void clearStatus();

    // bottomRight: the bottom-right corner of the central area (in window coords)
    void render(float bottomRightX, float bottomRightY);

private:
    bool m_visible = false;
    double m_showTime = 0.0;
    double m_lastTime = 0.0;

    float m_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    uint32_t m_index = 0;
    float m_time = 0.0f;

    std::string m_statusText;
};

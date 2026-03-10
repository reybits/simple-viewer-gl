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
#include <memory>

class cQuad;

class cProgress final
{
public:
    cProgress() = default;

    void init();
    void show();
    void hide();
    void render();

private:
    bool m_visible = false;
    double m_showTime = 0.0;
    double m_lastTime = 0.0;

    std::unique_ptr<cQuad> m_back;
    std::unique_ptr<cQuad> m_dot;
    float m_alpha[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    uint32_t m_index = 0;
    float m_time = 0.0f;
};

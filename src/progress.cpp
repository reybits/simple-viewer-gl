/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "progress.h"
#include "common/helpers.h"
#include "quad.h"
#include "types/types.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>

const float dotSize = 14.0f;
const float gap = 1.0f;
const float distance = dotSize + 2 * gap;

void cProgress::init()
{
    m_back.reset(new cQuad(distance * 2, distance * 2));
    m_back->setColor(cColor::Black);

    for (auto& dot : m_dot)
    {
        dot.dot.reset(new cQuad(dotSize, dotSize));
    }
}

void cProgress::render()
{
    if (m_visible)
    {
        const float dt = 1.0f / 30.0f;
        m_time -= dt;
        if (m_time <= 0.0f)
        {
            const float nextSpeed = 0.2f;
            m_time = nextSpeed;
            m_index = (m_index + 1) % helpers::countof(m_dot);
            m_dot[m_index].alpha = 255.0f;
        }

        int w, h;
        glfwGetFramebufferSize(render::getWindow(), &w, &h);

        const Vectorf pos{ w - distance * 2.0f, h - distance * 2.0f };

        m_back->render(pos);

        static const uint32_t idx[4] = { 0, 1, 3, 2 };
        for (size_t i = 0; i < helpers::countof(m_dot); i++)
        {
            auto& dot = m_dot[i];
            float alpha = dot.alpha;
            if (alpha > 0.0f)
            {
                const float alphaSpeed = 255.0f * 2.0f;
                alpha = std::max<float>(0.0f, alpha - dt * alphaSpeed);
                dot.alpha = alpha;
                dot.dot->setColor({ 255, 255, 255, (uint8_t)alpha });
                const Vectorf offset
                {
                    (idx[i] % 2) * distance + gap,
                    (idx[i] / 2) * distance + gap
                };
                dot.dot->render(pos + offset);
            }
        }
    }
}

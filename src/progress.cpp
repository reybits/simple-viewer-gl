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

#include <algorithm>

namespace
{
    constexpr auto DotSize = 100.0f;
    constexpr auto Gap = 20.0f;
    constexpr auto Distance = DotSize + 2.0f * Gap;

} // namespace

void cProgress::init()
{
    m_back.reset(new cQuad(Distance * 2.0f, Distance * 2.0f));
    m_back->setColor(cColor::Black);

    for (auto& dot : m_dot)
    {
        dot.dot.reset(new cQuad(DotSize, DotSize));
    }
}

void cProgress::render()
{
    if (m_visible)
    {
        const auto dt = 1.0f / 30.0f;
        m_time -= dt;
        if (m_time <= 0.0f)
        {
            constexpr auto NextSpeed = 0.3f;
            m_time = NextSpeed;
            m_index = (m_index + 1) % helpers::countof(m_dot);
            m_dot[m_index].alpha = 255.0f;
        }

        const auto& vp = render::getViewportSize();
        const Vectorf pos{ (vp.w - Distance * 2.0f) * 0.5f,
                           (vp.h - Distance * 2.0f) * 0.5f };

        m_back->render(pos);

        static constexpr uint32_t Idx[4] = { 0, 1, 3, 2 };
        for (size_t i = 0; i < helpers::countof(m_dot); i++)
        {
            auto& dot = m_dot[i];
            if (dot.alpha > 0.0f)
            {
                constexpr float AlphaSpeed = 255.0f * 2.0f;
                dot.alpha = std::max<float>(0.0f, dot.alpha - dt * AlphaSpeed);
                dot.dot->setColor({ 255, 255, 255, static_cast<uint8_t>(dot.alpha) });
                const Vectorf offset{
                    (Idx[i] % 2) * Distance + Gap,
                    std::floor(static_cast<float>(Idx[i]) / 2.0f) * Distance + Gap
                };
                dot.dot->render(pos + offset);
            }
        }
    }
}

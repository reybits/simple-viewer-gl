/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "Progress.h"
#include "Common/Timing.h"
#include "Quad.h"

#include <algorithm>
#include <iterator>

namespace
{
    constexpr auto DotSize = 30.0f;
    constexpr auto Gap = 4.0f;
    constexpr auto Distance = DotSize + 2.0f * Gap;

} // namespace

void cProgress::init()
{
    m_back = std::make_unique<cQuad>(Distance * 2.0f, Distance * 2.0f);
    m_back->setColor(cColor::Black);
    m_dot = std::make_unique<cQuad>(DotSize, DotSize);
}

void cProgress::show()
{
    m_visible = true;

    constexpr auto WaitTime = 2.0;
    m_showTime = timing::seconds() + WaitTime;
}

void cProgress::hide()
{
    m_visible = false;
}

void cProgress::render()
{
    if (m_visible)
    {
        static auto lastTime = timing::seconds();
        auto currentTime = timing::seconds();

        if (m_showTime > currentTime)
        {
            return;
        }

        auto dt = static_cast<float>(currentTime - lastTime);
        m_time -= dt;
        lastTime = currentTime;

        if (m_time <= 0.0f)
        {
            constexpr auto NextSpeed = 0.2f;
            m_time = NextSpeed;
            m_index = (m_index + 1) % std::size(m_alpha);
            m_alpha[m_index] = 255.0f;
        }

        const auto& vp = render::getViewportSize();
        const Vectorf pos{ (vp.w - Distance * 2.0f) * 0.5f,
                           (vp.h - Distance * 2.0f) * 0.5f };

        m_back->render(pos);

        static constexpr uint32_t Idx[4] = { 0, 1, 3, 2 };
        for (size_t i = 0; i < std::size(m_alpha); i++)
        {
            auto& alpha = m_alpha[i];
            if (alpha > 0.0f)
            {
                constexpr float AlphaSpeed = 255.0f * 2.0f;
                alpha = std::max(0.0f, alpha - dt * AlphaSpeed);
                m_dot->setColor({ 255, 255, 255, static_cast<uint8_t>(alpha) });
                const Vectorf offset{
                    (Idx[i] % 2) * Distance + Gap,
                    (Idx[i] / 2) * Distance + Gap
                };
                m_dot->render(pos + offset);
            }
        }
    }
}

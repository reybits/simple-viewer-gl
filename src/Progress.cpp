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

#include <algorithm>
#include <fmt/core.h>
#include <imgui/imgui.h>
#include <iterator>

namespace
{
    constexpr auto DotSize = 6.0f;
    constexpr auto DotGap = 4.0f;
    constexpr auto Margin = 10.0f;

} // namespace

void cProgress::show()
{
    m_visible = true;

    constexpr auto WaitTime = 2.0;
    auto now = timing::seconds();
    m_showTime = now + WaitTime;
    m_lastTime = now + WaitTime;
}

void cProgress::hide()
{
    m_visible = false;
    clearStatus();
}

void cProgress::setStatus(const std::string& text)
{
    m_statusText = text;
}

void cProgress::setPercent(float progress)
{
    m_statusText = fmt::format("{}%", static_cast<int>(progress * 100.0f));
}

void cProgress::clearStatus()
{
    m_statusText.clear();
}

void cProgress::render(float bottomRightX, float bottomRightY)
{
    if (m_visible == false)
    {
        return;
    }

    auto currentTime = timing::seconds();
    if (m_showTime > currentTime)
    {
        return;
    }

    auto dt = static_cast<float>(currentTime - m_lastTime);
    m_lastTime = currentTime;

    // Advance spinner animation
    m_time -= dt;
    if (m_time <= 0.0f)
    {
        constexpr auto NextSpeed = 0.2f;
        m_time = NextSpeed;
        m_index = (m_index + 1) % std::size(m_alpha);
        m_alpha[m_index] = 1.0f;
    }

    // Fade out dots
    for (auto& alpha : m_alpha)
    {
        if (alpha > 0.0f)
        {
            constexpr float AlphaSpeed = 2.0f;
            alpha = std::max(0.0f, alpha - dt * AlphaSpeed);
        }
    }

    // Calculate spinner block size: 4 dots in a 2x2 grid
    constexpr float spinnerW = DotSize * 2.0f + DotGap;
    constexpr float spinnerH = DotSize * 2.0f + DotGap;

    // Measure status text
    const bool hasText = m_statusText.empty() == false;
    ImVec2 textSize = { 0.0f, 0.0f };
    if (hasText)
    {
        textSize = ImGui::CalcTextSize(m_statusText.c_str());
    }

    // Window content: text on the left, spinner on the right
    const float contentW = spinnerW + (hasText ? textSize.x + DotGap * 2.0f : 0.0f);
    const float contentH = std::max(spinnerH, hasText ? textSize.y : 0.0f);

    constexpr auto flags = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_AlwaysAutoResize;

    auto& style = ImGui::GetStyle();
    const float winW = contentW + style.WindowPadding.x * 2.0f;
    const float winH = contentH + style.WindowPadding.y * 2.0f;

    ImGui::SetNextWindowPos(
        { bottomRightX - winW - Margin, bottomRightY - winH - Margin },
        ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);

    if (ImGui::Begin("##progress", nullptr, flags))
    {
        ImGui::Dummy({ contentW, contentH });

        auto* drawList = ImGui::GetWindowDrawList();
        ImVec2 winPos = ImGui::GetWindowPos();
        winPos.x += style.WindowPadding.x;
        winPos.y += style.WindowPadding.y;

        // Draw status text on the left
        float spinnerOffsetX = 0.0f;
        if (hasText)
        {
            auto textY = winPos.y + (contentH - textSize.y) * 0.5f;
            drawList->AddText({ winPos.x, textY }, IM_COL32(255, 255, 0, 255), m_statusText.c_str());
            spinnerOffsetX = textSize.x + DotGap * 2.0f;
        }

        // Draw 2x2 dot spinner on the right
        auto spinnerX = winPos.x + spinnerOffsetX;
        auto spinnerY = winPos.y + (contentH - spinnerH) * 0.5f;

        static constexpr uint32_t Idx[4] = { 0, 1, 3, 2 };
        for (size_t i = 0; i < std::size(m_alpha); i++)
        {
            float cx = spinnerX + (Idx[i] % 2u) * (DotSize + DotGap) + DotSize * 0.5f;
            float cy = spinnerY + (Idx[i] / 2u) * (DotSize + DotGap) + DotSize * 0.5f;

            auto alpha = m_alpha[i];
            constexpr float MinAlpha = 0.15f;
            alpha = MinAlpha + alpha * (1.0f - MinAlpha);

            auto color = IM_COL32(255, 255, 255, static_cast<uint8_t>(alpha * 255));
            drawList->AddRectFilled(
                { cx - DotSize * 0.5f, cy - DotSize * 0.5f },
                { cx + DotSize * 0.5f, cy + DotSize * 0.5f },
                color,
                DotSize * 0.25f);
        }
    }
    ImGui::End();
}

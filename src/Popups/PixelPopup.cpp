/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "PixelPopup.h"
#include "Renderer.h"

#include "img-icons.c"
#include "img-pointer-cross.c"

#include <cstring>
#include <fmt/core.h>
#include <imgui/imgui.h>

namespace
{
    constexpr ImVec4 ColorTransparent{ 0.0f, 0.0f, 0.0f, 0.0f };
    constexpr ImVec4 ColorGray{ 0.6f, 0.6f, 0.6f, 1.0f };
    constexpr ImVec4 ColorWhite{ 1.0f, 1.0f, 1.0f, 1.0f };

} // namespace

void cPixelPopup::init()
{
    m_pixelInfo.reset();

    m_pointer.reset(new cQuadSeries(imgPointerCross.width, imgPointerCross.height, imgPointerCross.pixel_data, imgPointerCross.bytes_per_pixel == 3 ? ePixelFormat::RGB : ePixelFormat::RGBA));
    m_pointer->setup(21, 21, 10);
    setCursor(0);

    m_icons.reset(new cQuadSeries(imgIcons.width, imgIcons.height, imgIcons.pixel_data, imgIcons.bytes_per_pixel == 3 ? ePixelFormat::RGB : ePixelFormat::RGBA));
    m_icons->setup(16, 16, 4);
}

void cPixelPopup::setPixelInfo(const sPixelInfo& pi)
{
    m_pixelInfo = pi;

    const bool isInside = isInsideImage(m_pixelInfo.point);

    m_info.clear();

    m_info.push_back({ Info::Icon::Position, isInside ? ColorWhite : ColorGray, fmt::format("{} x {}", static_cast<int>(pi.point.x), static_cast<int>(pi.point.y)), {} });

    if (isInside)
    {
        const auto& c = pi.color;
        m_info.push_back({ Info::Icon::Color, ColorWhite, fmt::format("rgba {:02X} {:02X} {:02X} {:02X}", c.r, c.g, c.b, c.a), {} });
    }
    else
    {
        m_info.push_back({ Info::Icon::Color, ColorGray, "rgba - - - -", {} });
    }

    auto& rc = m_pixelInfo.rc;
    if (rc.isSet())
    {
        rc.normalize();
        const int x = static_cast<int>(rc.tl.x);
        const int y = static_cast<int>(rc.tl.y);
        const int w = static_cast<int>(rc.width());
        const int h = static_cast<int>(rc.height());

        m_info.push_back({ Info::Icon::Size, ColorWhite, fmt::format("{} x {}", w, h), {} });

        m_info.push_back({ Info::Icon::Rect, ColorWhite, fmt::format("{}, {} -> {}, {}", x, y, x + w - 1, y + h - 1), {} });
    }
}

void cPixelPopup::render()
{
    renderCursor();
    renderInfo();
}

bool cPixelPopup::isInsideImage(const Vectorf& pos) const
{
    return !(pos.x < 0 || pos.y < 0 || pos.x >= m_pixelInfo.imgWidth || pos.y >= m_pixelInfo.imgHeight);
}

void cPixelPopup::setCursor(int cursor)
{
    m_pointer->setFrame(cursor);
}

void cPixelPopup::renderCursor()
{
    auto& pointerSize = m_pointer->getSize();
    const float x = std::round(m_pixelInfo.mouse.x - pointerSize.x * 0.5f);
    const float y = std::round(m_pixelInfo.mouse.y - pointerSize.y * 0.5f);
    m_pointer->render({ x, y });
}

void cPixelPopup::renderInfo()
{
    auto& io = ImGui::GetIO();
    float x = io.MousePos.x;
    float y = io.MousePos.y;
    float width = io.DisplaySize.x;
    float height = io.DisplaySize.y;

    const float offset = 10.0f;
    const ImVec2 pos{
        std::min<float>(x + offset, width - m_size.x),
        std::min<float>(y + offset, height - m_size.y)
    };

    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);

    const int flags = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_AlwaysAutoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("pixelinfo", nullptr, flags))
    {
        const auto& iconSize = m_icons->getSize();
        const ImVec2 size{ iconSize.x, iconSize.y };
        for (const auto& s : m_info)
        {
            m_icons->setFrame(static_cast<uint32_t>(s.icon));
            auto& quad = m_icons->getQuad();
            ImGui::ImageWithBg(reinterpret_cast<void*>(static_cast<uintptr_t>(quad.tex)), size, { quad.v[0].tx, quad.v[0].ty }, { quad.v[2].tx, quad.v[2].ty }, ColorTransparent, ColorGray);
            ImGui::SameLine();
            ImGui::TextColored(s.color, "%s", s.text.c_str());
        }
    }

    m_size = ImGui::GetWindowSize();

    ImGui::End();
}

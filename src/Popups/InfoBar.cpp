/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "InfoBar.h"
#include "Common/Config.h"
#include "Common/Unicode.h"

#include <cstring>
#include <fmt/core.h>
#include <imgui/imgui.h>

namespace
{
    constexpr ImVec4 ColorActive = { 1.0f, 1.0f, 0.0f, 1.0f };
    constexpr ImVec4 ColorCenter = { 0.1f, 1.0f, 0.1f, 1.0f };
    constexpr ImVec4 ColorDim = { 0.5f, 0.5f, 0.5f, 1.0f };

    const char* getHumanSize(float& size)
    {
        static const char* s[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB" };
        int idx = 0;
        for (; size > 1024.0f; size /= 1024.0f)
        {
            idx++;
        }
        return s[idx];
    }

    void separator()
    {
        ImGui::SameLine(0.0f, 0.0f);
        ImGui::TextColored(ColorDim, " | ");
        ImGui::SameLine(0.0f, 0.0f);
    }

} // namespace

cInfoBar::cInfoBar(const sConfig& config)
    : m_config(config)
{
}

void cInfoBar::render()
{
    constexpr auto flags = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoDocking;

    auto& io = ImGui::GetIO();
    auto& s = ImGui::GetStyle();
    auto font = ImGui::GetFont();
    const auto barHeight = s.WindowPadding.y * 2.0f + font->LegacySize;
    ImGui::SetNextWindowPos({ 0.0f, io.DisplaySize.y - barHeight });
    ImGui::SetNextWindowSize({ io.DisplaySize.x, barHeight });

    const auto oldRounding = s.WindowRounding;
    s.WindowRounding = 0.0f;

    if (ImGui::Begin("infobar", nullptr, flags))
    {
        auto color = m_config.centerWindow
            ? ColorCenter
            : ColorActive;

        if (m_fileIndex.empty() == false)
        {
            ImGui::TextColored(color, "%s", m_fileIndex.c_str());
            separator();
        }

        if (m_fileName.empty() == false)
        {
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextColored(color, "%s", m_fileName.c_str());
        }

        if (m_subImage.empty() == false)
        {
            separator();
            ImGui::TextColored(color, "%s", m_subImage.c_str());
        }

        if (m_format.empty() == false)
        {
            separator();
            ImGui::TextColored(color, "%s", m_format.c_str());
        }

        if (m_dimensions.empty() == false)
        {
            separator();
            ImGui::TextColored(color, "%s", m_dimensions.c_str());
        }

        if (m_scale.empty() == false)
        {
            separator();
            ImGui::TextColored(color, "%s", m_scale.c_str());
        }

        if (m_memory.empty() == false)
        {
            separator();
            ImGui::TextColored(color, "%s", m_memory.c_str());
        }
    }
    ImGui::End();

    s.WindowRounding = oldRounding;
}

void cInfoBar::setFileName(const char* path)
{
    m_fileName = shortenFilename(getFilename(path));
}

void cInfoBar::setFormat(const char* type)
{
    m_format = (type != nullptr) ? type : "unknown";
}

void cInfoBar::setDimensions(unsigned width, unsigned height, unsigned bpp)
{
    m_dimensions = (width > 0)
        ? fmt::format("{} x {} x {} bpp", width, height, bpp)
        : "";
}

void cInfoBar::setScale(float scale)
{
    m_scale = fmt::format("{:.1f}%", scale * 100.0f);
}

void cInfoBar::setFileIndex(unsigned index, unsigned count)
{
    m_fileIndex = (count > 1)
        ? fmt::format("{} / {}", index + 1, count)
        : "";
}

void cInfoBar::setSubImage(unsigned current, unsigned images)
{
    m_subImage = (images > 1)
        ? fmt::format("{} / {}", current + 1, images)
        : "";
}

void cInfoBar::setMemory(long fileSize, size_t memSize)
{
    auto fs = static_cast<float>(fileSize);
    auto fsu = getHumanSize(fs);
    auto ms = static_cast<float>(memSize);
    auto msu = getHumanSize(ms);
    m_memory = fmt::format("{:.1f} {} ({:.1f} {})", fs, fsu, ms, msu);
}

std::string cInfoBar::getFilename(const char* path) const
{
    std::string filename = "n/a";

    if (path != nullptr)
    {
        const char* n = ::strrchr(path, '/');
        if (n != nullptr)
        {
            path = n + 1;
        }

        filename = path;
    }

    return filename;
}

std::string cInfoBar::shortenFilename(const std::string& path) const
{
    std::string filename = path;

    const auto* s = reinterpret_cast<const uint8_t*>(path.c_str());
    const uint32_t count = countCodePoints(s);

    const uint32_t maxCount = m_config.fileMaxLength;
    if (count > maxCount)
    {
        uint32_t state = 0;
        uint32_t codepoint;

        filename.clear();
        for (uint32_t left = maxCount / 2; *s && left; s++)
        {
            filename += *s;
            if (!decode(&state, &codepoint, *s))
            {
                left--;
            }
        }

        const char* delim = "~";

        filename += delim;

        for (uint32_t skip = count - (maxCount - ::strlen(delim)); *s && skip; s++)
        {
            if (!decode(&state, &codepoint, *s))
            {
                skip--;
            }
        }

        filename += reinterpret_cast<const char*>(s);
    }

    return filename;
}

/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "infobar.h"
#include "common/config.h"
#include "common/unicode.h"
#include "imgui/imgui.h"
#include "renderer.h"

#include "common/timing.h"

#include <cstring>
#include <fmt/format.h>

namespace
{
    constexpr ImVec4 ColorCenter = { 0.1f, 1.0f, 0.1f, 1.0f };
    constexpr ImVec4 ColorYellow = { 1.0f, 1.0f, 0.0f, 1.0f };

} // namespace

cInfoBar::cInfoBar(const sConfig& config)
    : m_config(config)
{
}

void cInfoBar::render()
{
    auto& io = ImGui::GetIO();
    int width = static_cast<int>(io.DisplaySize.x);
    int height = static_cast<int>(io.DisplaySize.y);

    auto& s = ImGui::GetStyle();
    auto font = ImGui::GetFont();
    const float h = s.WindowPadding.y * 2.0f + font->LegacySize;

    ImGui::SetNextWindowPos({ 0.0f, height - h }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ static_cast<float>(width), h }, ImGuiCond_Always);
    constexpr auto flags = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoSavedSettings;

    const auto oldRounding = s.WindowRounding;
    s.WindowRounding = 0.0f;
    if (ImGui::Begin("infobar", nullptr, flags))
    {
        auto color = m_config.centerWindow
            ? ColorCenter
            : ColorYellow;
        ImGui::TextColored(color, "%s", m_bottominfo.c_str());

        if (m_progressText.empty() == false)
        {
            const float textWidth = ImGui::CalcTextSize(m_progressText.c_str()).x;
            ImGui::SameLine(static_cast<float>(width) - textWidth - s.WindowPadding.x);
            ImGui::TextColored(ColorYellow, "%s", m_progressText.c_str());
        }
    }
    ImGui::End();

    if (m_config.debug)
    {
        static unsigned frame = 0;
        static float fps = 0.0f;

        frame++;
        static auto last = timing::seconds();
        const auto now = timing::seconds();
        const auto delta = now - last;
        if (delta > 0.5f)
        {
            fps = frame / delta;
            last = now;
            frame = 0;
        }

        ImGui::SetNextWindowPos({ 0.0f, 0.0f }, ImGuiCond_Always);
        if (ImGui::Begin("debug", nullptr, flags | ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::TextColored(ColorYellow, "fps: %.1f", fps);
        }
        ImGui::End();
    }

    s.WindowRounding = oldRounding;
}

void cInfoBar::setProgressText(const std::string& text)
{
    m_progressText = text;
}

void cInfoBar::setProgressPercent(float progress)
{
    m_progressText = fmt::format("[{}%]", static_cast<int>(progress * 100.0f));
}

void cInfoBar::clearProgress()
{
    m_progressText.clear();
}

void cInfoBar::setInfo(const sInfo& p)
{
    const auto fileName = getFilename(p.path);
    const auto shortName = shortenFilename(fileName);

    std::string idxImg;
    if (p.files_count > 1)
    {
        idxImg = fmt::format("{} out {} | ", p.index + 1, p.files_count);
    }

    std::string subImage;
    if (p.images > 1)
    {
        subImage = fmt::format(" | {} / {}", p.current + 1, p.images);
    }

    auto file_size = static_cast<float>(p.file_size);
    auto file_s = getHumanSize(file_size);
    auto mem_size = static_cast<float>(p.mem_size);
    auto mem_s = getHumanSize(mem_size);

    const char* type = p.type != nullptr ? p.type : "unknown";
    m_bottominfo = fmt::format("{}{}{} | {} | {} x {} x {} bpp | {:.1f}% | {:.1f} {} ({:.1f} {})",
                               idxImg, shortName, subImage,
                               type, p.width, p.height, p.bpp, p.scale * 100.0f,
                               file_size, file_s, mem_size, mem_s);
}

const char* cInfoBar::getHumanSize(float& size)
{
    static const char* s[] = { "B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB", "ZiB", "YiB" };
    int idx = 0;
    for (; size > 1024.0f; size /= 1024.0f)
    {
        idx++;
    }

    return s[idx];
}

const std::string cInfoBar::getFilename(const char* path) const
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

const std::string cInfoBar::shortenFilename(const std::string& path) const
{
    std::string filename = path;

    // ::printf("'%s' -> ", path);

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
        // ::printf("'%s'\n", filename.c_str());
    }

    return filename;
}

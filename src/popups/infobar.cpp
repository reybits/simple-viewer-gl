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

#include <cstring>

namespace
{
    const ImVec4 ColorYellow = { 1.0f, 1.0f, 0.0f, 1.0f };
}

cInfoBar::cInfoBar(const sConfig& config)
    : m_config(config)
{
}

void cInfoBar::render()
{
    int width;
    int height;
    glfwGetWindowSize(cRenderer::getWindow(), &width, &height);

    auto& s = ImGui::GetStyle();
    auto font = ImGui::GetFont();
    const float h = s.WindowPadding.y * 2.0f + font->LegacySize;

    ImGui::SetNextWindowPos({ 0.0f, height - h }, ImGuiCond_Always);
    ImGui::SetNextWindowSize({ (float)width, h }, ImGuiCond_Always);
    constexpr auto flags = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoSavedSettings;

    const auto oldRounding = s.WindowRounding;
    s.WindowRounding = 0.0f;
    if (ImGui::Begin("infobar", nullptr, flags))
    {
        ImGui::TextColored(ColorYellow, "%s", m_bottominfo.c_str());
    }
    ImGui::End();

    if (m_config.debug)
    {
        static unsigned frame = 0;
        static float fps = 0.0f;

        frame++;
        static auto last = glfwGetTime();
        const auto now = glfwGetTime();
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

void cInfoBar::setInfo(const sInfo& p)
{
    const auto fileName = getFilename(p.path);
    glfwSetWindowTitle(cRenderer::getWindow(), fileName.c_str());

    char idx_img[20] = { 0 };
    if (p.files_count > 1)
    {
        ::snprintf(idx_img, sizeof(idx_img), "%u out %u | ", p.index + 1, p.files_count);
    }

    char sub_image[20] = { 0 };
    if (p.images > 1)
    {
        ::snprintf(sub_image, sizeof(sub_image), " | %u / %u", p.current + 1, p.images);
    }

    auto file_size = static_cast<float>(p.file_size);
    auto file_s = getHumanSize(file_size);
    auto mem_size = static_cast<float>(p.mem_size);
    auto mem_s = getHumanSize(mem_size);

    char title[1000] = { 0 };
    const auto shortName = shortenFilename(fileName);
    ::snprintf(title, sizeof(title), "%s%s%s | %s | %u x %u x %u bpp | %.1f%% | %.1f %s (%.1f %s)",
               idx_img, shortName.c_str(), sub_image,
               p.type, p.width, p.height, p.bpp, p.scale * 100.0f,
               file_size, file_s, mem_size, mem_s);

    m_bottominfo = title;
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

    const uint8_t* s = (uint8_t*)path.c_str();
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

        filename += (const char*)s;
        // ::printf("'%s'\n", filename.c_str());
    }

    return filename;
}

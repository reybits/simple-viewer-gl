/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "HelpPopup.h"
#include "imgui/imgui.h"

namespace
{
    constexpr ImVec4 ColorKey{ 1.0f, 1.0f, 0.5f, 1.0f };
    constexpr ImVec4 ColorDesc{ 1.0f, 1.0f, 1.0f, 1.0f };
    constexpr ImVec4 ColorSection{ 0.6f, 0.8f, 1.0f, 1.0f };

    struct KeyBinding
    {
        const char* key;
        const char* description;
    };

    constexpr float KeyColumnWidth = 180.0f;

    void RenderSection(const char* title, const KeyBinding* bindings, int count)
    {
        ImGui::Spacing();
        ImGui::TextColored(ColorSection, "%s", title);
        ImGui::Separator();
        for (int i = 0; i < count; i++)
        {
            ImGui::TextColored(ColorKey, "  %s", bindings[i].key);
            ImGui::SameLine(KeyColumnWidth);
            ImGui::TextColored(ColorDesc, "%s", bindings[i].description);
        }
    }

    constexpr KeyBinding GeneralBindings[] = {
        { "<?>", "show / hide help" },
        { "<esc> / <q>", "exit" },
        { "<o>", "open file" },
        { "<enter>", "toggle fullscreen / windowed mode" },
    };

    constexpr KeyBinding NavigationBindings[] = {
        { "<space>", "next image" },
        { "<backspace>", "previous image" },
        { "<home>", "first image" },
        { "<end>", "last image" },
        { "<pgup> / <pgdn>", "previous / next subimage" },
    };

    constexpr KeyBinding ViewBindings[] = {
        { "<+> / <->", "zoom in / out" },
        { "<1>...<0>", "set scale from 100% to 1000%" },
        { "<s>", "fit image to window" },
        { "<shift>+<s>", "toggle 'keep scale' on image load" },
        { "<r>", "rotate clockwise" },
        { "<shift>+<r>", "rotate counterclockwise" },
        { "<f>", "flip horizontal" },
        { "<shift>+<f>", "flip vertical" },
    };

    constexpr KeyBinding PanBindings[] = {
        { "<arrows> / <h/j/k/l>", "pan image by pixel" },
        { "<shift>+<arrows/hjkl>", "pan image by step" },
    };

    constexpr KeyBinding DisplayBindings[] = {
        { "<c>", "cycle background" },
        { "<shift>+<c>", "toggle center window on image" },
        { "<i>", "show / hide on-screen info" },
        { "<e>", "show / hide exif" },
        { "<p>", "show / hide pixel info" },
        { "<b>", "show / hide border around image" },
        { "<g>", "show / hide pixel grid" },
    };

    constexpr KeyBinding FileBindings[] = {
        { "<del>", "toggle deletion mark" },
        { "<ctrl>+<del>", "delete marked images from disk" },
    };

} // namespace

void cHelpPopup::render()
{
    if (m_isVisible)
    {
        constexpr auto flags = ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_AlwaysAutoResize;

        ImGuiWindowClass windowClass;
        windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_AutoHideTabBar;
        ImGui::SetNextWindowClass(&windowClass);

        if (ImGui::Begin("Help", nullptr, flags))
        {
            RenderSection("General", GeneralBindings, IM_ARRAYSIZE(GeneralBindings));
            RenderSection("Navigation", NavigationBindings, IM_ARRAYSIZE(NavigationBindings));
            RenderSection("View", ViewBindings, IM_ARRAYSIZE(ViewBindings));
            RenderSection("Pan", PanBindings, IM_ARRAYSIZE(PanBindings));
            RenderSection("Display", DisplayBindings, IM_ARRAYSIZE(DisplayBindings));
            RenderSection("File Management", FileBindings, IM_ARRAYSIZE(FileBindings));
        }
        ImGui::End();
    }
}

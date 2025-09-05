/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "helppopup.h"
#include "imgui/imgui.h"
#include "types/vector.h"

namespace
{
    const ImVec4 ColorKey{ 1.0f, 1.0f, 0.5, 1.0f };
    const ImVec4 ColorDesc{ 1.0f, 1.0f, 1.0f, 1.0f };

    struct KeyBinding
    {
        const char* key;
        const char* description;
    };

    const KeyBinding KeyBindingsList[] = {
        { "<esc> / <q>", "exit" },
        { "<space>", "next image" },
        { "<backspace>", "previous image" },
        { "<+> / <->", "scale image" },
        { "<1>...<0>", "set scale from 100% to 1000%" },
        { "<enter>", "switch fullscreen / windowed mode" },
        { "<del>", "toggle deletion mark" },
        { "<ctrl>+<del>", "delete marked images from disk" },
        { "<r>", "rotate clockwise" },
        { "<shift>+<r>", "rotate counterclockwise" },
        { "<pgup> / <bgdn>", "previous /next subimage" },
        { "<s>", "fit image to window" },
        { "<shift>+<s>", "toggle 'keep scale' on image load" },
        { "<c>", "hide / show chequerboard" },
        { "<i>", "hide / show on-screen info" },
        { "<e>", "hide / show exif" },
        { "<p>", "hide / show pixel info" },
        { "<b>", "hide / show border around image" },
    };

} // namespace

void cHelpPopup::render()
{
    if (m_isVisible)
    {
        constexpr auto flags = ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_AlwaysAutoResize;
        if (ImGui::Begin("Help", nullptr, flags))
        {
            for (const auto& s : KeyBindingsList)
            {
                ImGui::TextColored(ColorKey, "%s", s.key);
                ImGui::SameLine(120.0f);
                ImGui::Bullet();
                ImGui::TextColored(ColorDesc, "%s", s.description);
            }
        }
        ImGui::End();
    }
}

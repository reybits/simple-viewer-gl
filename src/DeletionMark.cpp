/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "DeletionMark.h"

#include <imgui/imgui.h>

void cDeletionMark::render()
{
    constexpr auto flags = ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoScrollbar
        | ImGuiWindowFlags_NoSavedSettings
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoFocusOnAppearing
        | ImGuiWindowFlags_NoNav
        | ImGuiWindowFlags_AlwaysAutoResize;

    constexpr float Margin = 10.0f;
    ImGui::SetNextWindowPos({ Margin, Margin }, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.6f);

    if (ImGui::Begin("##deletion_mark", nullptr, flags))
    {
        constexpr float Scale = 2.0f;
        ImGui::SetWindowFontScale(Scale);
        ImGui::TextColored({ 1.0f, 0.3f, 0.3f, 1.0f }, "\xef\x8b\xad"); // U+F2ED fa-trash-can
        ImGui::SetWindowFontScale(1.0f);
    }
    ImGui::End();
}

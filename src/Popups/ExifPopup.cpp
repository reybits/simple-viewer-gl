/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "ExifPopup.h"
#include "imgui/imgui.h"

namespace
{
    const ImVec4 tagColor{ 1.0f, 1.0f, 0.5f, 1.0f };
    const ImVec4 valueColor{ 1.0f, 1.0f, 1.0f, 1.0f };

} // namespace

void cExifPopup::setExifList(const sBitmapDescription::ExifList& exifList)
{
    m_exif = exifList;
}

void cExifPopup::render()
{
    if (m_exif.size())
    {
        const int flags = ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_AlwaysAutoResize
            | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing;

        ImGui::SetNextWindowPos({ 5.0f, 5.0f }, ImGuiCond_Once);
        if (ImGui::Begin("EXIF", nullptr, flags))
        {
            if (ImGui::BeginTable("exif_table", 2))
            {
                for (const auto& s : m_exif)
                {
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextColored(tagColor, "%s", s.tag.c_str());
                    ImGui::TableSetColumnIndex(1);
                    ImGui::TextColored(valueColor, "%s", s.value.c_str());
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }
}

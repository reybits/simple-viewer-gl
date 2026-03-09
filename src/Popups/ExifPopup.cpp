/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "ExifPopup.h"

#include <cctype>
#include <imgui/imgui.h>

namespace
{
    constexpr ImVec4 TagColor{ 1.0f, 1.0f, 0.5f, 1.0f };
    constexpr ImVec4 ValueColor{ 1.0f, 1.0f, 1.0f, 1.0f };
    constexpr ImVec4 EmptyColor{ 0.5f, 0.5f, 0.5f, 1.0f };

    bool MatchesFilter(const char* text, const char* filter)
    {
        for (const char* h = text; *h != '\0'; ++h)
        {
            const char* hi = h;
            const char* ni = filter;
            while (*ni != '\0'
                && std::tolower(static_cast<unsigned char>(*hi)) == std::tolower(static_cast<unsigned char>(*ni)))
            {
                ++hi;
                ++ni;
            }
            if (*ni == '\0')
            {
                return true;
            }
        }
        return false;
    }

    bool MatchesFilter(const sBitmapDescription::ExifEntry& entry, const char* filter, const char* categoryName)
    {
        return MatchesFilter(entry.tag.c_str(), filter)
            || MatchesFilter(entry.value.c_str(), filter)
            || MatchesFilter(categoryName, filter);
    }

} // namespace

const char* cExifPopup::categoryName(eCategory category)
{
    constexpr const char* Names[] = {
        "Camera",
        "Exposure",
        "Image",
        "Date",
        "Software",
        "Info",
        "Other",
    };
    static_assert(std::size(Names) == static_cast<size_t>(eCategory::Count));

    auto idx = static_cast<size_t>(category);
    return idx < std::size(Names) ? Names[idx] : "Other";
}

void cExifPopup::setExifList(const sBitmapDescription::ExifList& exifList)
{
    m_exif = exifList;
    m_filter[0] = '\0';
    rebuildGroups();
}

void cExifPopup::setExifList(sBitmapDescription::ExifList&& exifList)
{
    m_exif = std::move(exifList);
    m_filter[0] = '\0';
    rebuildGroups();
}

void cExifPopup::rebuildGroups()
{
    for (size_t i = 0; i < CategoryCount; i++)
    {
        m_groups[i].category = static_cast<eCategory>(i);
        m_groups[i].entries.clear();
    }

    for (const auto& entry : m_exif)
    {
        auto idx = static_cast<size_t>(entry.category);
        if (idx >= CategoryCount)
        {
            idx = static_cast<size_t>(eCategory::Other);
        }
        m_groups[idx].entries.push_back(&entry);
    }
}

void cExifPopup::render()
{
    constexpr int flags = ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoFocusOnAppearing;

    if (ImGui::Begin("Metadata", nullptr, flags) == false)
    {
        ImGui::End();
        return;
    }

    if (m_exif.empty())
    {
        ImGui::TextColored(EmptyColor, "No metadata available.");
        ImGui::End();
        return;
    }

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##filter", "Filter...", m_filter, sizeof(m_filter));

    ImGui::Separator();

    const bool hasFilter = m_filter[0] != '\0';

    if (ImGui::BeginChild("##entries", { 0.0f, 0.0f }))
    {
        for (const auto& group : m_groups)
        {
            if (group.entries.empty())
            {
                continue;
            }

            const auto* name = categoryName(group.category);

            // Skip groups with no matching entries when filtering
            if (hasFilter)
            {
                bool hasMatch = false;
                for (const auto* entry : group.entries)
                {
                    if (MatchesFilter(*entry, m_filter, name))
                    {
                        hasMatch = true;
                        break;
                    }
                }
                if (hasMatch == false)
                {
                    continue;
                }
            }

            if (ImGui::CollapsingHeader(name, ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::BeginTable("##tbl", 2, ImGuiTableFlags_SizingStretchProp))
                {
                    ImGui::TableSetupColumn("Tag", ImGuiTableColumnFlags_WidthFixed);
                    ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

                    for (const auto* entry : group.entries)
                    {
                        if (hasFilter && MatchesFilter(*entry, m_filter, name) == false)
                        {
                            continue;
                        }

                        ImGui::TableNextRow();
                        ImGui::TableSetColumnIndex(0);
                        ImGui::TextColored(TagColor, "%s", entry->tag.c_str());

                        ImGui::TableSetColumnIndex(1);
                        ImGui::TextColored(ValueColor, "%s", entry->value.c_str());

                        // Copy value to clipboard on click
                        if (ImGui::IsItemHovered())
                        {
                            ImGui::SetTooltip("Click to copy");
                            if (ImGui::IsMouseClicked(0))
                            {
                                ImGui::SetClipboardText(entry->value.c_str());
                            }
                        }
                    }

                    ImGui::EndTable();
                }
            }
        }
    }
    ImGui::EndChild();

    ImGui::End();
}

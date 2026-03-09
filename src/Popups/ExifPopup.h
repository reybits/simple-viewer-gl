/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Popup.h"

#include "Common/BitmapDescription.h"

#include <vector>

class cExifPopup final : public cPopup
{
public:
    void render() override;

    void setExifList(const sBitmapDescription::ExifList& exifList);
    void setExifList(sBitmapDescription::ExifList&& exifList);

private:
    using eCategory = sBitmapDescription::ExifCategory;

    struct sGroup
    {
        eCategory category;
        std::vector<const sBitmapDescription::ExifEntry*> entries;
    };

    void rebuildGroups();
    static const char* categoryName(eCategory category);

    static constexpr auto CategoryCount = static_cast<size_t>(eCategory::Count);

    sBitmapDescription::ExifList m_exif;
    sGroup m_groups[CategoryCount];
    char m_filter[128] = {};
};

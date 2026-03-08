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

class cExifPopup final : public cPopup
{
public:
    void render() override;

    void setExifList(const sBitmapDescription::ExifList& exifList);

private:
    sBitmapDescription::ExifList m_exif;
};

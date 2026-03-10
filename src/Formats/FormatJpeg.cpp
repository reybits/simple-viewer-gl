/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatJpeg.h"
#include "Common/Callbacks.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/Helpers.h"
#include "Common/ImageInfo.h"
#include "Log/Log.h"

#include <cstring>

#if defined(EXIF_SUPPORT)
#include <libexif/exif-data.h>
#endif

namespace
{
#if defined(EXIF_SUPPORT)
    using eCategory = sImageInfo::ExifCategory;

    void AddExifTag(ExifData* d, ExifIfd ifd, ExifTag tag, eCategory category, sImageInfo::ExifList& exifList)
    {
        ExifEntry* entry = exif_content_get_entry(d->ifd[ifd], tag);
        if (entry != nullptr)
        {
            char buf[1024];
            exif_entry_get_value(entry, buf, sizeof(buf));

            helpers::trimRightSpaces(buf);
            if (*buf)
            {
                exifList.push_back({ category, exif_tag_get_title_in_ifd(tag, ifd), buf });
            }
        }
    }
#endif

} // namespace

bool cFormatJpeg::isSupported(cFile& file, Buffer& buffer) const
{
    if (readBuffer(file, buffer, 4) == false)
    {
        return false;
    }

    const auto h = reinterpret_cast<const uint8_t*>(buffer.data());

    constexpr struct
    {
        uint8_t four[4];
    } Heads[] = {
        { { 0xff, 0xd8, 0xff, 0xdb } },
        { { 0xff, 0xd8, 0xff, 0xe0 } },
        { { 0xff, 0xd8, 0xff, 0xed } },
        { { 0xff, 0xd8, 0xff, 0xe1 } },
        { { 0xff, 0xd8, 0xff, 0xe2 } },
        { { 0xff, 0xd8, 0xff, 0xee } },
        { { 0xff, 0xd8, 0xff, 0xfe } },
    };

    for (auto& header : Heads)
    {
        if (std::memcmp(h, header.four, sizeof(header)) == 0)
        {
            return true;
        }
    }

    return false;
}

bool cFormatJpeg::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (openFile(file, filename, info) == false)
    {
        return false;
    }

    const auto size = file.getSize();
    Buffer in(size);
    file.read(in.data(), size);

    auto progressCb = [this](float p) { updateProgress(p); };
    auto allocatedCb = [this]() { signalBitmapAllocated(); };
    auto imageInfoCb = [this]() { signalImageInfo(); };
    auto previewCb = [this, &chunk](cJpegDecoder::Bitmap&& thumb) {
        sPreviewData preview;
        preview.bitmap = std::move(thumb.data);
        preview.width = thumb.width;
        preview.height = thumb.height;
        preview.pitch = thumb.pitch;
        preview.bpp = thumb.bpp;
        preview.format = thumb.format;
        preview.fullImageWidth = chunk.width;
        preview.fullImageHeight = chunk.height;
        signalPreviewReady(std::move(preview));
    };
    auto result = m_decoder.decodeJpeg(in.data(), static_cast<uint32_t>(size), chunk, info, progressCb, allocatedCb, imageInfoCb, previewCb, m_stop);
    if (result.success == false)
    {
        return false;
    }

    // ICC LUT generated inside decodeJpeg() — applied on GPU during rendering.

#if defined(EXIF_SUPPORT)
    ExifData* ed = nullptr;
    if (result.exifData.empty() == false)
    {
        ed = exif_data_new_from_data(result.exifData.data(), static_cast<unsigned>(result.exifData.size()));
    }
    if (ed != nullptr)
    {
        auto& exifList = info.exifList;

        // Camera
        AddExifTag(ed, EXIF_IFD_0, EXIF_TAG_MAKE, eCategory::Camera, exifList);
        AddExifTag(ed, EXIF_IFD_0, EXIF_TAG_MODEL, eCategory::Camera, exifList);
        AddExifTag(ed, EXIF_IFD_0, EXIF_TAG_SOFTWARE, eCategory::Camera, exifList);
        AddExifTag(ed, EXIF_IFD_0, EXIF_TAG_ORIENTATION, eCategory::Camera, exifList);

        // Exposure
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_TIME, eCategory::Exposure, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_FNUMBER, eCategory::Exposure, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_MAX_APERTURE_VALUE, eCategory::Exposure, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_FOCAL_LENGTH, eCategory::Exposure, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_MODE, eCategory::Exposure, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_PROGRAM, eCategory::Exposure, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_ISO_SPEED_RATINGS, eCategory::Exposure, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_FLASH, eCategory::Exposure, exifList);

        // Image
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_PIXEL_X_DIMENSION, eCategory::Image, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_PIXEL_Y_DIMENSION, eCategory::Image, exifList);
        AddExifTag(ed, EXIF_IFD_0, EXIF_TAG_X_RESOLUTION, eCategory::Image, exifList);
        AddExifTag(ed, EXIF_IFD_0, EXIF_TAG_Y_RESOLUTION, eCategory::Image, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_COLOR_SPACE, eCategory::Image, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_WHITE_BALANCE, eCategory::Image, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_CONTRAST, eCategory::Image, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_SATURATION, eCategory::Image, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_SHARPNESS, eCategory::Image, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_SCENE_CAPTURE_TYPE, eCategory::Image, exifList);
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_DIGITAL_ZOOM_RATIO, eCategory::Image, exifList);

        // Date
        AddExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_DATE_TIME_ORIGINAL, eCategory::Date, exifList);

        // Store EXIF orientation for renderer
        ExifEntry* orientEntry = exif_content_get_entry(ed->ifd[EXIF_IFD_0], EXIF_TAG_ORIENTATION);
        if (orientEntry != nullptr)
        {
            auto byteOrder = exif_data_get_byte_order(ed);
            info.exifOrientation = exif_get_short(orientEntry->data, byteOrder);
        }

        exif_data_unref(ed);
    }
#endif

    return true;
}

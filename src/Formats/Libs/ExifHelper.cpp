/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "ExifHelper.h"

#if defined(EXIF_SUPPORT)

#include "Common/Helpers.h"
#include "Log/Log.h"

#include <libexif/exif-data.h>

namespace
{
    using ExifCategory = sImageInfo::ExifCategory;

    ExifCategory CategoryForTag(ExifTag tag, ExifIfd ifd)
    {
        if (ifd == EXIF_IFD_GPS)
        {
            return ExifCategory::GPS;
        }

        switch (tag)
        {
        // Camera / lens hardware
        case EXIF_TAG_MAKE:
        case EXIF_TAG_MODEL:
        case EXIF_TAG_CAMERA_OWNER_NAME:
        case EXIF_TAG_BODY_SERIAL_NUMBER:
        case EXIF_TAG_LENS_SPECIFICATION:
        case EXIF_TAG_LENS_MAKE:
        case EXIF_TAG_LENS_MODEL:
        case EXIF_TAG_LENS_SERIAL_NUMBER:
            return ExifCategory::Camera;

        // Exposure / shooting settings
        case EXIF_TAG_EXPOSURE_TIME:
        case EXIF_TAG_FNUMBER:
        case EXIF_TAG_EXPOSURE_PROGRAM:
        case EXIF_TAG_ISO_SPEED_RATINGS:
        case EXIF_TAG_SENSITIVITY_TYPE:
        case EXIF_TAG_STANDARD_OUTPUT_SENSITIVITY:
        case EXIF_TAG_RECOMMENDED_EXPOSURE_INDEX:
        case EXIF_TAG_ISO_SPEED:
        case EXIF_TAG_SHUTTER_SPEED_VALUE:
        case EXIF_TAG_APERTURE_VALUE:
        case EXIF_TAG_BRIGHTNESS_VALUE:
        case EXIF_TAG_EXPOSURE_BIAS_VALUE:
        case EXIF_TAG_MAX_APERTURE_VALUE:
        case EXIF_TAG_METERING_MODE:
        case EXIF_TAG_LIGHT_SOURCE:
        case EXIF_TAG_FLASH:
        case EXIF_TAG_FOCAL_LENGTH:
        case EXIF_TAG_FLASH_ENERGY:
        case EXIF_TAG_EXPOSURE_INDEX:
        case EXIF_TAG_EXPOSURE_MODE:
        case EXIF_TAG_WHITE_BALANCE:
        case EXIF_TAG_DIGITAL_ZOOM_RATIO:
        case EXIF_TAG_FOCAL_LENGTH_IN_35MM_FILM:
        case EXIF_TAG_SCENE_CAPTURE_TYPE:
        case EXIF_TAG_GAIN_CONTROL:
        case EXIF_TAG_CONTRAST:
        case EXIF_TAG_SATURATION:
        case EXIF_TAG_SHARPNESS:
        case EXIF_TAG_SUBJECT_DISTANCE:
        case EXIF_TAG_SUBJECT_DISTANCE_RANGE:
        case EXIF_TAG_SUBJECT_AREA:
        case EXIF_TAG_SUBJECT_LOCATION:
        case EXIF_TAG_SENSING_METHOD:
        case EXIF_TAG_SCENE_TYPE:
        case EXIF_TAG_CUSTOM_RENDERED:
        case EXIF_TAG_SPECTRAL_SENSITIVITY:
        case EXIF_TAG_FOCAL_PLANE_X_RESOLUTION:
        case EXIF_TAG_FOCAL_PLANE_Y_RESOLUTION:
        case EXIF_TAG_FOCAL_PLANE_RESOLUTION_UNIT:
            return ExifCategory::Exposure;

        // Date / time
        case EXIF_TAG_DATE_TIME:
        case EXIF_TAG_DATE_TIME_ORIGINAL:
        case EXIF_TAG_DATE_TIME_DIGITIZED:
        case EXIF_TAG_OFFSET_TIME:
        case EXIF_TAG_OFFSET_TIME_ORIGINAL:
        case EXIF_TAG_OFFSET_TIME_DIGITIZED:
        case EXIF_TAG_SUB_SEC_TIME:
        case EXIF_TAG_SUB_SEC_TIME_ORIGINAL:
        case EXIF_TAG_SUB_SEC_TIME_DIGITIZED:
        case EXIF_TAG_TIME_ZONE_OFFSET:
            return ExifCategory::Date;

        // Software / processing
        case EXIF_TAG_SOFTWARE:
        case EXIF_TAG_PRINT_IMAGE_MATCHING:
        case EXIF_TAG_FILE_SOURCE:
            return ExifCategory::Software;

        // Authorship / description
        case EXIF_TAG_ARTIST:
        case EXIF_TAG_COPYRIGHT:
        case EXIF_TAG_IMAGE_DESCRIPTION:
        case EXIF_TAG_DOCUMENT_NAME:
        case EXIF_TAG_USER_COMMENT:
        case EXIF_TAG_MAKER_NOTE:
        case EXIF_TAG_IMAGE_UNIQUE_ID:
        case EXIF_TAG_XP_TITLE:
        case EXIF_TAG_XP_COMMENT:
        case EXIF_TAG_XP_AUTHOR:
        case EXIF_TAG_XP_KEYWORDS:
        case EXIF_TAG_XP_SUBJECT:
            return ExifCategory::Info;

        // Image properties
        case EXIF_TAG_IMAGE_WIDTH:
        case EXIF_TAG_IMAGE_LENGTH:
        case EXIF_TAG_BITS_PER_SAMPLE:
        case EXIF_TAG_COMPRESSION:
        case EXIF_TAG_PHOTOMETRIC_INTERPRETATION:
        case EXIF_TAG_ORIENTATION:
        case EXIF_TAG_SAMPLES_PER_PIXEL:
        case EXIF_TAG_X_RESOLUTION:
        case EXIF_TAG_Y_RESOLUTION:
        case EXIF_TAG_RESOLUTION_UNIT:
        case EXIF_TAG_PLANAR_CONFIGURATION:
        case EXIF_TAG_YCBCR_COEFFICIENTS:
        case EXIF_TAG_YCBCR_SUB_SAMPLING:
        case EXIF_TAG_YCBCR_POSITIONING:
        case EXIF_TAG_COLOR_SPACE:
        case EXIF_TAG_PIXEL_X_DIMENSION:
        case EXIF_TAG_PIXEL_Y_DIMENSION:
        case EXIF_TAG_EXIF_VERSION:
        case EXIF_TAG_FLASH_PIX_VERSION:
        case EXIF_TAG_COMPONENTS_CONFIGURATION:
        case EXIF_TAG_COMPRESSED_BITS_PER_PIXEL:
        case EXIF_TAG_GAMMA:
        case EXIF_TAG_WHITE_POINT:
        case EXIF_TAG_PRIMARY_CHROMATICITIES:
        case EXIF_TAG_REFERENCE_BLACK_WHITE:
        case EXIF_TAG_TRANSFER_FUNCTION:
        case EXIF_TAG_TRANSFER_RANGE:
        case EXIF_TAG_NEW_CFA_PATTERN:
        case EXIF_TAG_CFA_PATTERN:
        case EXIF_TAG_CFA_REPEAT_PATTERN_DIM:
            return ExifCategory::Image;

        default: // Do nothing
            break;
        }

        return ExifCategory::Other;
    }

    struct ForeachCtx
    {
        sImageInfo::ExifList* list;
        ExifIfd ifd;
    };

    void ForeachEntry(ExifEntry* entry, void* userData)
    {
        auto* ctx = static_cast<ForeachCtx*>(userData);

        char buf[1024];
        exif_entry_get_value(entry, buf, sizeof(buf));
        helpers::trimRightSpaces(buf);

        if (*buf == '\0')
        {
            return;
        }

        const char* title = exif_tag_get_title_in_ifd(entry->tag, ctx->ifd);
        if (title == nullptr)
        {
            return;
        }

        ctx->list->push_back({ CategoryForTag(entry->tag, ctx->ifd), title, buf });
    }

} // namespace

void exif::extractAll(const uint8_t* data, unsigned size, sImageInfo::ExifList& exifList, uint16_t& orientation)
{
    auto* ed = exif_data_new_from_data(data, size);
    if (ed == nullptr)
    {
        cLog::Debug("EXIF: exif_data_new_from_data failed.");
        return;
    }

    // Iterate all entries in IFDs we care about (skip IFD_1 = thumbnail).
    constexpr ExifIfd Ifds[] = {
        EXIF_IFD_0,
        EXIF_IFD_EXIF,
        EXIF_IFD_GPS,
        EXIF_IFD_INTEROPERABILITY,
    };

    for (auto ifd : Ifds)
    {
        ForeachCtx ctx{ &exifList, ifd };
        exif_content_foreach_entry(ed->ifd[ifd], ForeachEntry, &ctx);
    }

    // Extract orientation separately (needed for render transform).
    auto* orientEntry = exif_content_get_entry(ed->ifd[EXIF_IFD_0], EXIF_TAG_ORIENTATION);
    if (orientEntry != nullptr)
    {
        auto byteOrder = exif_data_get_byte_order(ed);
        orientation    = exif_get_short(orientEntry->data, byteOrder);
    }

    exif_data_unref(ed);
}

#else

void exif::extractAll(const uint8_t*, unsigned, sImageInfo::ExifList&, uint16_t&)
{
}

#endif

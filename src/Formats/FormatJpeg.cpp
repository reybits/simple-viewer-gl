/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatJpeg.h"
#include "Cms/Cms.h"
#include "Common/BitmapDescription.h"
#include "Common/File.h"
#include "Common/Helpers.h"
#include "Formats/Libs/JpegDecoder.h"
#include "Log/Log.h"

#include <cstring>
#include <jpeglib.h>
#include <setjmp.h>

#if defined(EXIF_SUPPORT)
#include <libexif/exif-data.h>
#endif

namespace
{
#if defined(EXIF_SUPPORT)
    void addExifTag(ExifData* d, ExifIfd ifd, ExifTag tag, sBitmapDescription::ExifList& exifList)
    {
        ExifEntry* entry = exif_content_get_entry(d->ifd[ifd], tag);
        if (entry != nullptr)
        {
            // Get the contents of the tag in human-readable form
            char buf[1024];
            exif_entry_get_value(entry, buf, sizeof(buf));

            helpers::trimRightSpaces(buf);
            if (*buf)
            {
                exifList.push_back({ exif_tag_get_title_in_ifd(tag, ifd), buf });
            }
        }
    }

#if defined(DEBUG)
    void printExifTag(const ExifData* d, ExifIfd ifd, ExifTag tag)
    {
        ExifEntry* entry = exif_content_get_entry(d->ifd[ifd], tag);
        if (entry != nullptr)
        {
            // Get the contents of the tag in human-readable form
            char buf[1024];
            exif_entry_get_value(entry, buf, sizeof(buf));

            // Don't bother printing it if it's entirely blank
            helpers::trimRightSpaces(buf);
            if (*buf != 0)
            {
                cLog::Debug("ifd {} , tag {:#06x} : '{}': '{}'", static_cast<int>(ifd), static_cast<unsigned>(tag), exif_tag_get_name_in_ifd(tag, ifd), buf);
            }
        }
    }

    void dumpAllExif(const ExifData* ed)
    {
        const ExifTag Tags[] = {
            EXIF_TAG_INTEROPERABILITY_INDEX,
            EXIF_TAG_INTEROPERABILITY_VERSION,
            EXIF_TAG_NEW_SUBFILE_TYPE,
            EXIF_TAG_IMAGE_WIDTH,
            EXIF_TAG_IMAGE_LENGTH,
            EXIF_TAG_BITS_PER_SAMPLE,
            EXIF_TAG_COMPRESSION,
            EXIF_TAG_PHOTOMETRIC_INTERPRETATION,
            EXIF_TAG_FILL_ORDER,
            EXIF_TAG_DOCUMENT_NAME,
            EXIF_TAG_IMAGE_DESCRIPTION,
            EXIF_TAG_MAKE,
            EXIF_TAG_MODEL,
            EXIF_TAG_STRIP_OFFSETS,
            EXIF_TAG_ORIENTATION,
            EXIF_TAG_SAMPLES_PER_PIXEL,
            EXIF_TAG_ROWS_PER_STRIP,
            EXIF_TAG_STRIP_BYTE_COUNTS,
            EXIF_TAG_X_RESOLUTION,
            EXIF_TAG_Y_RESOLUTION,
            EXIF_TAG_PLANAR_CONFIGURATION,
            EXIF_TAG_RESOLUTION_UNIT,
            EXIF_TAG_TRANSFER_FUNCTION,
            EXIF_TAG_SOFTWARE,
            EXIF_TAG_DATE_TIME,
            EXIF_TAG_ARTIST,
            EXIF_TAG_WHITE_POINT,
            EXIF_TAG_PRIMARY_CHROMATICITIES,
            EXIF_TAG_SUB_IFDS,
            EXIF_TAG_TRANSFER_RANGE,
            EXIF_TAG_JPEG_PROC,
            EXIF_TAG_JPEG_INTERCHANGE_FORMAT,
            EXIF_TAG_JPEG_INTERCHANGE_FORMAT_LENGTH,
            EXIF_TAG_YCBCR_COEFFICIENTS,
            EXIF_TAG_YCBCR_SUB_SAMPLING,
            EXIF_TAG_YCBCR_POSITIONING,
            EXIF_TAG_REFERENCE_BLACK_WHITE,
            EXIF_TAG_XML_PACKET,
            EXIF_TAG_RELATED_IMAGE_FILE_FORMAT,
            EXIF_TAG_RELATED_IMAGE_WIDTH,
            EXIF_TAG_RELATED_IMAGE_LENGTH,
            EXIF_TAG_CFA_REPEAT_PATTERN_DIM,
            EXIF_TAG_CFA_PATTERN,
            EXIF_TAG_BATTERY_LEVEL,
            EXIF_TAG_COPYRIGHT,
            EXIF_TAG_EXPOSURE_TIME,
            EXIF_TAG_FNUMBER,
            EXIF_TAG_IPTC_NAA,
            EXIF_TAG_IMAGE_RESOURCES,
            EXIF_TAG_EXIF_IFD_POINTER,
            EXIF_TAG_INTER_COLOR_PROFILE,
            EXIF_TAG_EXPOSURE_PROGRAM,
            EXIF_TAG_SPECTRAL_SENSITIVITY,
            EXIF_TAG_GPS_INFO_IFD_POINTER,
            EXIF_TAG_ISO_SPEED_RATINGS,
            EXIF_TAG_OECF,
            EXIF_TAG_TIME_ZONE_OFFSET,
            EXIF_TAG_EXIF_VERSION,
            EXIF_TAG_DATE_TIME_ORIGINAL,
            EXIF_TAG_DATE_TIME_DIGITIZED,
            EXIF_TAG_COMPONENTS_CONFIGURATION,
            EXIF_TAG_COMPRESSED_BITS_PER_PIXEL,
            EXIF_TAG_SHUTTER_SPEED_VALUE,
            EXIF_TAG_APERTURE_VALUE,
            EXIF_TAG_BRIGHTNESS_VALUE,
            EXIF_TAG_EXPOSURE_BIAS_VALUE,
            EXIF_TAG_MAX_APERTURE_VALUE,
            EXIF_TAG_SUBJECT_DISTANCE,
            EXIF_TAG_METERING_MODE,
            EXIF_TAG_LIGHT_SOURCE,
            EXIF_TAG_FLASH,
            EXIF_TAG_FOCAL_LENGTH,
            EXIF_TAG_SUBJECT_AREA,
            EXIF_TAG_TIFF_EP_STANDARD_ID,
            EXIF_TAG_MAKER_NOTE,
            EXIF_TAG_USER_COMMENT,
            EXIF_TAG_SUB_SEC_TIME,
            EXIF_TAG_SUB_SEC_TIME_ORIGINAL,
            EXIF_TAG_SUB_SEC_TIME_DIGITIZED,
            EXIF_TAG_XP_TITLE,
            EXIF_TAG_XP_COMMENT,
            EXIF_TAG_XP_AUTHOR,
            EXIF_TAG_XP_KEYWORDS,
            EXIF_TAG_XP_SUBJECT,
            EXIF_TAG_FLASH_PIX_VERSION,
            EXIF_TAG_COLOR_SPACE,
            EXIF_TAG_PIXEL_X_DIMENSION,
            EXIF_TAG_PIXEL_Y_DIMENSION,
            EXIF_TAG_RELATED_SOUND_FILE,
            EXIF_TAG_INTEROPERABILITY_IFD_POINTER,
            EXIF_TAG_FLASH_ENERGY,
            EXIF_TAG_SPATIAL_FREQUENCY_RESPONSE,
            EXIF_TAG_FOCAL_PLANE_X_RESOLUTION,
            EXIF_TAG_FOCAL_PLANE_Y_RESOLUTION,
            EXIF_TAG_FOCAL_PLANE_RESOLUTION_UNIT,
            EXIF_TAG_SUBJECT_LOCATION,
            EXIF_TAG_EXPOSURE_INDEX,
            EXIF_TAG_SENSING_METHOD,
            EXIF_TAG_FILE_SOURCE,
            EXIF_TAG_SCENE_TYPE,
            EXIF_TAG_NEW_CFA_PATTERN,
            EXIF_TAG_CUSTOM_RENDERED,
            EXIF_TAG_EXPOSURE_MODE,
            EXIF_TAG_WHITE_BALANCE,
            EXIF_TAG_DIGITAL_ZOOM_RATIO,
            EXIF_TAG_FOCAL_LENGTH_IN_35MM_FILM,
            EXIF_TAG_SCENE_CAPTURE_TYPE,
            EXIF_TAG_GAIN_CONTROL,
            EXIF_TAG_CONTRAST,
            EXIF_TAG_SATURATION,
            EXIF_TAG_SHARPNESS,
            EXIF_TAG_DEVICE_SETTING_DESCRIPTION,
            EXIF_TAG_SUBJECT_DISTANCE_RANGE,
            EXIF_TAG_IMAGE_UNIQUE_ID,
            EXIF_TAG_GAMMA,
            EXIF_TAG_PRINT_IMAGE_MATCHING,
            EXIF_TAG_PADDING,
        };
        for (int i = 0; i < EXIF_IFD_COUNT; i++)
        {
            for (auto tag : Tags)
            {
                printExifTag(ed, (ExifIfd)i, tag);
            }
        }
    }
#endif
#endif
} // namespace

bool cFormatJpeg::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, 4))
    {
        return false;
    }

    const auto h = reinterpret_cast<const uint8_t*>(buffer.data());

    struct Head
    {
        uint8_t four[4];
    };

    const Head Heads[] = {
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
        if (::memcmp(h, header.four, sizeof(header)) == 0)
        {
            return true;
        }
    }

    return false;
}

bool cFormatJpeg::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    cFile file;
    if (!openFile(file, filename, desc))
    {
        return false;
    }

    uint32_t size = file.getSize();
    Buffer in(size);
    file.read(in.data(), size);

    bool result = decodeJpeg(in.data(), size, desc);
    if (result)
    {
#if defined(EXIF_SUPPORT)
        auto ed = exif_data_new_from_file(filename);
        if (ed != nullptr)
        {
#if defined(DEBUG)
            dumpAllExif(ed);
#endif

            auto& exifList = desc.exifList;

            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_DATE_TIME_ORIGINAL, exifList);
            addExifTag(ed, EXIF_IFD_0, EXIF_TAG_MAKE, exifList);
            addExifTag(ed, EXIF_IFD_0, EXIF_TAG_MODEL, exifList);
            addExifTag(ed, EXIF_IFD_0, EXIF_TAG_SOFTWARE, exifList);
            addExifTag(ed, EXIF_IFD_0, EXIF_TAG_ORIENTATION, exifList);
            addExifTag(ed, EXIF_IFD_0, EXIF_TAG_X_RESOLUTION, exifList);
            addExifTag(ed, EXIF_IFD_0, EXIF_TAG_Y_RESOLUTION, exifList);

            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_MAX_APERTURE_VALUE, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_FNUMBER, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_FOCAL_LENGTH, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_TIME, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_MODE, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_EXPOSURE_PROGRAM, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_ISO_SPEED_RATINGS, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_PIXEL_X_DIMENSION, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_PIXEL_Y_DIMENSION, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_WHITE_BALANCE, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_COLOR_SPACE, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_DIGITAL_ZOOM_RATIO, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_FLASH, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_SCENE_CAPTURE_TYPE, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_CONTRAST, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_SATURATION, exifList);
            addExifTag(ed, EXIF_IFD_EXIF, EXIF_TAG_SHARPNESS, exifList);

            exif_data_unref(ed);
        }
#endif
    }

    return result;
}

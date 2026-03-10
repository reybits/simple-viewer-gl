/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#if defined(HEIF_SUPPORT)

#include "FormatHeif.h"
#include "Common/Callbacks.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/Helpers.h"
#include "Common/ImageInfo.h"
#include "Log/Log.h"

#include <cstring>
#include <libheif/heif.h>

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

bool cFormatHeif::isSupported(cFile& file, Buffer& buffer) const
{
    if (readBuffer(file, buffer, 12) == false)
    {
        return false;
    }

    auto result = heif_check_filetype(reinterpret_cast<const uint8_t*>(buffer.data()), static_cast<int>(buffer.size()));
    return result == heif_filetype_yes_supported || result == heif_filetype_maybe;
}

bool cFormatHeif::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (openFile(file, filename, info) == false)
    {
        return false;
    }

    // Read entire file into memory for libheif
    Buffer fileData;
    fileData.resize(file.getSize());
    if (file.read(fileData.data(), file.getSize()) != file.getSize())
    {
        cLog::Error("Can't read HEIF file.");
        return false;
    }

    auto ctx = heif_context_alloc();
    if (ctx == nullptr)
    {
        cLog::Error("Can't allocate HEIF context.");
        return false;
    }

    auto err = heif_context_read_from_memory_without_copy(ctx, fileData.data(), fileData.size(), nullptr);
    if (err.code != heif_error_Ok)
    {
        cLog::Error("Can't parse HEIF file: {}.", err.message);
        heif_context_free(ctx);
        return false;
    }

    // Only the primary image is currently loaded
    info.images = 1;
    info.current = 0;

    // Get primary image handle
    heif_image_handle* handle = nullptr;
    err = heif_context_get_primary_image_handle(ctx, &handle);
    if (err.code != heif_error_Ok)
    {
        cLog::Error("Can't get HEIF image handle: {}.", err.message);
        heif_context_free(ctx);
        return false;
    }

    const bool hasAlpha = heif_image_handle_has_alpha_channel(handle) != 0;
    const auto chroma = hasAlpha
        ? heif_chroma_interleaved_RGBA
        : heif_chroma_interleaved_RGB;

    // Decode to interleaved RGB/RGBA
    heif_image* img = nullptr;
    err = heif_decode_image(handle, &img, heif_colorspace_RGB, chroma, nullptr);
    if (err.code != heif_error_Ok)
    {
        cLog::Error("Can't decode HEIF image: {}.", err.message);
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        return false;
    }

    const int width = heif_image_get_width(img, heif_channel_interleaved);
    const int height = heif_image_get_height(img, heif_channel_interleaved);

    if (width <= 0 || height <= 0)
    {
        cLog::Error("Invalid HEIF image dimensions: {}x{}.", width, height);
        heif_image_release(img);
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        return false;
    }

    chunk.width = static_cast<uint32_t>(width);
    chunk.height = static_cast<uint32_t>(height);

    const uint32_t bpp = hasAlpha ? 32 : 24;
    const auto format = hasAlpha
        ? ePixelFormat::RGBA
        : ePixelFormat::RGB;
    info.bppImage = bpp;

    setupBitmap(chunk, info, bpp, format, "heif");

    // Copy decoded pixels into chunk bitmap
    int stride = 0;
    const uint8_t* srcData = heif_image_get_plane_readonly(img, heif_channel_interleaved, &stride);
    if (srcData == nullptr)
    {
        cLog::Error("Can't get HEIF pixel data.");
        heif_image_release(img);
        heif_image_handle_release(handle);
        heif_context_free(ctx);
        return false;
    }

    const uint32_t bytesPerPixel = bpp / 8;
    const uint32_t copyBytes = chunk.width * bytesPerPixel;
    for (uint32_t row = 0; row < chunk.height; row++)
    {
        std::memcpy(
            chunk.bitmap.data() + row * chunk.pitch,
            srcData + row * stride,
            copyBytes);
    }

    // Extract ICC profile
    auto profileType = heif_image_handle_get_color_profile_type(handle);
    if (profileType == heif_color_profile_type_rICC || profileType == heif_color_profile_type_prof)
    {
        auto profileSize = heif_image_handle_get_raw_color_profile_size(handle);
        if (profileSize > 0)
        {
            std::vector<uint8_t> iccData(profileSize);
            err = heif_image_handle_get_raw_color_profile(handle, iccData.data());
            if (err.code == heif_error_Ok)
            {
                if (applyIccProfile(chunk, iccData.data(), static_cast<uint32_t>(profileSize)))
                {
                    info.formatName = "heif/icc";
                }
            }
        }
    }

    // Extract EXIF metadata
#if defined(EXIF_SUPPORT)
    {
        const int metaCount = heif_image_handle_get_number_of_metadata_blocks(handle, "Exif");
        if (metaCount > 0)
        {
            heif_item_id metaId;
            heif_image_handle_get_list_of_metadata_block_IDs(handle, "Exif", &metaId, 1);

            auto metaSize = heif_image_handle_get_metadata_size(handle, metaId);
            if (metaSize > 0)
            {
                std::vector<uint8_t> exifRaw(metaSize);
                err = heif_image_handle_get_metadata(handle, metaId, exifRaw.data());
                if (err.code == heif_error_Ok)
                {
                    // HEIF EXIF metadata has a 4-byte offset prefix before the TIFF header
                    const uint8_t* exifData = exifRaw.data();
                    size_t exifSize = exifRaw.size();
                    if (exifSize > 4)
                    {
                        auto tiffOffset = static_cast<size_t>(helpers::read_uint32(exifData));
                        if (tiffOffset + 4 < exifSize)
                        {
                            exifData += 4 + tiffOffset;
                            exifSize -= 4 + tiffOffset;
                        }
                        else
                        {
                            exifData += 4;
                            exifSize -= 4;
                        }
                    }

                    auto ed = exif_data_new_from_data(exifData, static_cast<unsigned>(exifSize));
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
                }
            }
        }
    }
#endif

    heif_image_release(img);
    heif_image_handle_release(handle);
    heif_context_free(ctx);

    return true;
}

#endif

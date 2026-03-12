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
#include "Common/ImageInfo.h"
#include "Libs/ExifHelper.h"
#include "Log/Log.h"

#include <cstring>
#include <libheif/heif.h>

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
                    // HEIF EXIF metadata has a 4-byte big-endian offset prefix;
                    // the rest (including "Exif\0\0" header) is passed to libexif as-is.
                    // libheif already applies orientation transforms during decode,
                    // so extract tags for display but ignore the orientation value.
                    if (exifRaw.size() > 4)
                    {
                        uint16_t unusedOrientation = 0;
                        exif::extractAll(exifRaw.data() + 4,
                                         static_cast<unsigned>(exifRaw.size() - 4),
                                         info.exifList, unusedOrientation);
                    }
                }
            }
        }
    }

    heif_image_release(img);
    heif_image_handle_release(handle);
    heif_context_free(ctx);

    return true;
}

#endif

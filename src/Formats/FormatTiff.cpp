/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#if defined(TIFF_SUPPORT)

#include "FormatTiff.h"
#include "Common/Callbacks.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
#include "Log/Log.h"

#include <cstring>
#include <stdarg.h>
#include <tiffio.h>

namespace
{
    void ErrorHandler(const char*, const char*, va_list)
    {
        // ::printf("(EE) \n");
    }

    void WarningHandler(const char*, const char*, va_list)
    {
        // ::printf("(WW) \n");
    }

} // namespace

bool cFormatTiff::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, sizeof(uint32_t)))
    {
        return false;
    }

    const auto h = buffer.data();
    const uint8_t le[4] = { 0x49, 0x49, 0x2A, 0x00 };
    const uint8_t be[4] = { 0x4D, 0x4D, 0x00, 0x2A };
    return !::memcmp(h, le, sizeof(le)) || !::memcmp(h, be, sizeof(be));
}

bool cFormatTiff::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    m_filename = filename;
    return load(0, chunk, info);
}

bool cFormatTiff::LoadSubImageImpl(uint32_t current, sChunkData& chunk, sImageInfo& info)
{
    return load(current, chunk, info);
}

void cFormatTiff::decodePreview(void* handle, uint32_t fullWidth, uint32_t fullHeight, unsigned current)
{
    auto tif = static_cast<TIFF*>(handle);
    const auto numDirs = TIFFNumberOfDirectories(tif);

    // Scan directories for a reduced-resolution image
    for (uint16_t dir = 0; dir < numDirs; dir++)
    {
        if (dir == current)
        {
            continue;
        }

        if (TIFFSetDirectory(tif, dir) == 0)
        {
            continue;
        }

        uint32_t subfileType = 0;
        TIFFGetField(tif, TIFFTAG_SUBFILETYPE, &subfileType);
        if ((subfileType & FILETYPE_REDUCEDIMAGE) == 0)
        {
            continue;
        }

        TIFFRGBAImage img;
        char emsg[1024];
        if (TIFFRGBAImageBegin(&img, tif, 0, emsg) == 0)
        {
            continue;
        }

        const uint32_t width = img.width;
        const uint32_t height = img.height;

        // Only use as preview if it's actually smaller
        if (width >= fullWidth && height >= fullHeight)
        {
            TIFFRGBAImageEnd(&img);
            continue;
        }

        constexpr uint32_t bpp = 32;
        const uint32_t pitch = width * (bpp / 8);

        sPreviewData data;
        data.width = width;
        data.height = height;
        data.pitch = pitch;
        data.bpp = bpp;
        data.format = ePixelFormat::RGBA;
        data.fullImageWidth = fullWidth;
        data.fullImageHeight = fullHeight;
        data.bitmap.resize(pitch * height);

        img.req_orientation = ORIENTATION_TOPLEFT;
        if (TIFFRGBAImageGet(&img, reinterpret_cast<uint32_t*>(data.bitmap.data()), width, height) != 0)
        {
            cLog::Debug("TIFF preview: {}x{} (dir {}), full: {}x{}", width, height, dir, fullWidth, fullHeight);
            signalPreviewReady(std::move(data));
            TIFFRGBAImageEnd(&img);
            break;
        }

        TIFFRGBAImageEnd(&img);
    }

    // Restore the original directory
    TIFFSetDirectory(tif, current);
}

bool cFormatTiff::load(uint32_t current, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (!openFile(file, m_filename.c_str(), info))
    {
        return false;
    }

    file.close();

    bool result = false;

    TIFFSetErrorHandler(ErrorHandler);
    TIFFSetWarningHandler(WarningHandler);

    auto tif = TIFFOpen(m_filename.c_str(), "r");
    if (tif != nullptr)
    {
        // read count of pages in image
        info.images = TIFFNumberOfDirectories(tif);
        info.current = std::min(current, info.images - 1);

        // set desired page
        if (TIFFSetDirectory(tif, info.current) != 0)
        {
            TIFFRGBAImage img;
            char emsg[1024];
            if (TIFFRGBAImageBegin(&img, tif, 0, emsg) != 0)
            {
                chunk.width = img.width;
                chunk.height = img.height;
                info.bppImage = img.bitspersample * img.samplesperpixel;

                TIFFRGBAImageEnd(&img);
                decodePreview(tif, chunk.width, chunk.height, info.current);

                // Re-open the image after preview scan may have changed directory
                if (TIFFSetDirectory(tif, info.current) == 0
                    || TIFFRGBAImageBegin(&img, tif, 0, emsg) == 0)
                {
                    TIFFClose(tif);
                    return false;
                }

                // Read ICC data after directory is restored — TIFFGetField
                // returns pointers into TIFF's internal memory that are only
                // valid for the current directory.
                uint32_t iccProfileSize = 0;
                void* iccProfile = nullptr;
                bool hasIccProfile = TIFFGetField(tif, TIFFTAG_ICCPROFILE, &iccProfileSize, &iccProfile) != 0
                    && iccProfileSize > 0 && iccProfile != nullptr;

                float* chr = nullptr;
                float* wp = nullptr;
                uint16_t* gmr = nullptr;
                uint16_t* gmg = nullptr;
                uint16_t* gmb = nullptr;
                bool hasIccTables = false;
                if (hasIccProfile == false)
                {
                    if (TIFFGetField(tif, TIFFTAG_PRIMARYCHROMATICITIES, &chr) && chr != nullptr
                        && TIFFGetField(tif, TIFFTAG_WHITEPOINT, &wp) && wp != nullptr)
                    {
                        TIFFGetFieldDefaulted(tif, TIFFTAG_TRANSFERFUNCTION, &gmr, &gmg, &gmb);
                        hasIccTables = true;
                    }
                }

                setupBitmap(chunk, info, 32, ePixelFormat::RGBA, "tiff");

                img.req_orientation = ORIENTATION_TOPLEFT;

                auto bitmap = chunk.bitmap.data();
                result = TIFFRGBAImageGet(&img, reinterpret_cast<uint32_t*>(bitmap), chunk.width, chunk.height) != 0;

                if (result)
                {
                    bool iccApplied = false;
                    if (hasIccProfile)
                    {
                        iccApplied = applyIccProfile(chunk, iccProfile, iccProfileSize);
                    }
                    else if (hasIccTables)
                    {
                        iccApplied = applyIccProfile(chunk, chr, wp, gmr, gmg, gmb);
                    }
                    if (iccApplied)
                    {
                        info.formatName = "tiff/icc";
                    }
                }

                TIFFRGBAImageEnd(&img);
            }
            else
            {
                cLog::Error("TIFF error: '{}'.", emsg);
            }
        }

        TIFFClose(tif);
    }

    return result;
}

#endif

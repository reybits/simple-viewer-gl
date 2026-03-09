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
#include "Common/BitmapDescription.h"
#include "Common/Callbacks.h"
#include "Common/File.h"
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

bool cFormatTiff::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    m_filename = filename;
    return load(0, desc);
}

bool cFormatTiff::LoadSubImageImpl(unsigned current, sBitmapDescription& desc)
{
    return load(current, desc);
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

bool cFormatTiff::load(unsigned current, sBitmapDescription& desc)
{
    cFile file;
    if (!openFile(file, m_filename.c_str(), desc))
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
        desc.images = TIFFNumberOfDirectories(tif);
        desc.current = std::min(current, desc.images - 1);

        // set desired page
        if (TIFFSetDirectory(tif, desc.current) != 0)
        {
            struct Icc
            {
                bool hasEmbeded() const
                {
                    return profileSize && profile != nullptr;
                }

                uint32_t profileSize = 0;
                void* profile = nullptr;

                bool hasTables() const
                {
                    return chr != nullptr && wp != nullptr && gmr != nullptr && gmg != nullptr && gmb != nullptr;
                }

                float* chr = nullptr;
                float* wp = nullptr;
                uint16_t* gmr = nullptr;
                uint16_t* gmg = nullptr;
                uint16_t* gmb = nullptr;
            };

            Icc icc;

            if (TIFFGetField(tif, TIFFTAG_ICCPROFILE, &icc.profileSize, &icc.profile) == 0)
            {
                if (TIFFGetField(tif, TIFFTAG_PRIMARYCHROMATICITIES, &icc.chr))
                {
                    if (TIFFGetField(tif, TIFFTAG_WHITEPOINT, &icc.wp))
                    {
                        TIFFGetFieldDefaulted(tif, TIFFTAG_TRANSFERFUNCTION, &icc.gmr, &icc.gmg, &icc.gmb);
                    }
                }
            }

            TIFFRGBAImage img;
            char emsg[1024];
            if (TIFFRGBAImageBegin(&img, tif, 0, emsg) != 0)
            {
                desc.width = img.width;
                desc.height = img.height;
                desc.bppImage = img.bitspersample * img.samplesperpixel;

                TIFFRGBAImageEnd(&img);
                decodePreview(tif, desc.width, desc.height, desc.current);

                // Re-open the image after preview scan may have changed directory
                if (TIFFSetDirectory(tif, desc.current) == 0
                    || TIFFRGBAImageBegin(&img, tif, 0, emsg) == 0)
                {
                    TIFFClose(tif);
                    return false;
                }

                setupBitmap(desc, desc.width, desc.height, 32, ePixelFormat::RGBA, "tiff");

                // set desired orientation
                img.req_orientation = ORIENTATION_TOPLEFT;

                auto bitmap = desc.bitmap.data();
                result = TIFFRGBAImageGet(&img, (uint32_t*)bitmap, desc.width, desc.height) != 0;

                if (result)
                {
                    bool iccApplied = false;
                    if (icc.hasEmbeded())
                    {
                        iccApplied = applyIccProfile(desc, icc.profile, icc.profileSize);
                    }
                    else if (icc.hasTables())
                    {
                        iccApplied = applyIccProfile(desc, icc.chr, icc.wp, icc.gmr, icc.gmg, icc.gmb);
                    }
                    if (iccApplied)
                    {
                        desc.formatName = "tiff/icc";
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

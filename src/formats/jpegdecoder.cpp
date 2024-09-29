/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "jpegdecoder.h"
#include "common/bitmap_description.h"
#include "common/buffer.h"
#include "common/helpers.h"

#include <jpeglib.h>
#include <setjmp.h>
#include <string.h>

namespace
{
#if 0
    const char* GetFormat(uint32_t idx)
    {
        const char* Formats[] = {
            "jpeg", // JCS_UNKNOWN:
            "jpeg/grayscale", // JCS_GRAYSCALE:
            "jpeg/rgb", // JCS_RGB:
            "jpeg/ycbcr", // JCS_YCbCr:
            "jpeg/cmyk", // JCS_CMYK:
            "jpeg/ycck", // JCS_YCCK:
        };
        const auto size = helpers::countof(Formats);
        return Formats[idx < size ? idx : 0];
    }
#endif

    struct sErrorMgr
    {
        struct jpeg_error_mgr pub; /* "public" fields */
        jmp_buf setjmp_buffer;     /* for return to caller */
    };

    void ErrorExit(j_common_ptr cinfo)
    {
        // cinfo->err really points to a sErrorMgr struct, so coerce pointer
        auto errMgr = reinterpret_cast<sErrorMgr*>(cinfo->err);

        // Always display the message.
        // We could postpone this until after returning, if we chose.
        (*cinfo->err->output_message)(cinfo);

        // Return control to the setjmp point
        longjmp(errMgr->setjmp_buffer, 1);
    }

    const uint32_t MaxMarkerLength = 0xffff;

} // namespace

cJpegDecoder::cJpegDecoder(iCallbacks* callbacks)
    : cFormat(callbacks)
    , JPEG_EXIF(JPEG_APP0 + 1)
    , JPEG_ICCP(JPEG_APP0 + 2)
{
}

bool cJpegDecoder::decodeJpeg(const uint8_t* in, uint32_t size, sBitmapDescription& desc)
{
    // Step 1: allocate and initialize JPEG decompression object

    // This struct contains the JPEG decompression parameters and pointers to
    // working space (which is allocated as needed by the JPEG library).
    jpeg_decompress_struct cinfo;

    /* We use our private extension JPEG error handler.
     * Note that this struct must live as long as the main JPEG parameter
     * struct, to avoid dangling-pointer problems.
     */
    sErrorMgr jerr;

    // We set up the normal JPEG error routines, then override error_exit.
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = ErrorExit;
    // Establish the setjmp return context for ErrorExit to use.
    if (setjmp(jerr.setjmp_buffer))
    {
        // If we get here, the JPEG code has signaled an error.
        // We need to clean up the JPEG object, close the input file, and return.
        jpeg_destroy_decompress(&cinfo);
        return false;
    }
    // Now we can initialize the JPEG decompression object.
    jpeg_create_decompress(&cinfo);

    // Step 2: specify data source (eg, a file)
    jpeg_mem_src(&cinfo, (uint8_t*)in, size);

    // Step 3: read file parameters with jpeg_read_header()
    setupMarkers(&cinfo);
    jpeg_read_header(&cinfo, TRUE);

    // unsigned exifSize = 0;
    // auto exif = locateExif(cinfo, exifSize);
    // if (exif != nullptr)
    // {
    // ::printf("-- exif located, size %u\n%s\n", exifSize, exif);
    // }

    /* Step 4: set parameters for decompression */
    const bool isCMYK = cinfo.jpeg_color_space == JCS_CMYK || cinfo.jpeg_color_space == JCS_YCCK;
    if (isCMYK == false)
    {
        cinfo.out_color_space = JCS_RGB; // convert to RGB
    }

    /* Step 5: Start decompressor */
    jpeg_start_decompress(&cinfo);

    desc.size = size;

    desc.width = cinfo.output_width;
    desc.height = cinfo.output_height;
    desc.format = GL_RGB;

    /* Step 6: while (scan lines remain to be read) */
    /*           jpeg_read_scanlines(...); */

    /* Here we use the library's state variable cinfo.output_scanline as the
     * loop counter, so that we don't have to keep track ourselves.
     */

    // m_formatName = GetFormat(cinfo.jpeg_color_space);

    Icc icc;
    locateICCProfile(cinfo, icc);

    if (isCMYK)
    {
        desc.bpp = 24;
        desc.bppImage = 32;
        desc.pitch = helpers::calculatePitch(desc.width, desc.bpp);
        desc.bitmap.resize(desc.pitch * desc.height);

        auto out = desc.bitmap.data();

        Buffer buffer(helpers::calculatePitch(desc.width, desc.bppImage));
        auto input = buffer.data();
        while (cinfo.output_scanline < cinfo.output_height && m_stop == false)
        {
            jpeg_read_scanlines(&cinfo, &input, 1);

            for (uint32_t x = 0; x < desc.width; x++)
            {
                const uint32_t src = x * 4;
                const double C = 1.0 - input[src + 0] / 255.0; // C
                const double M = 1.0 - input[src + 1] / 255.0; // M
                const double Y = 1.0 - input[src + 2] / 255.0; // Y
                const double K = 1.0 - input[src + 3] / 255.0; // K
                const double Kinv = 1.0 - K;

                const uint32_t dst = x * 3;
                out[dst + 0] = (uint8_t)((1.0 - (C * Kinv + K)) * 255.0);
                out[dst + 1] = (uint8_t)((1.0 - (M * Kinv + K)) * 255.0);
                out[dst + 2] = (uint8_t)((1.0 - (Y * Kinv + K)) * 255.0);
            }

            out += desc.pitch;

            auto progress = static_cast<float>(cinfo.output_scanline) / cinfo.output_height;
            updateProgress(progress);
        }
    }
    else
    {
        desc.bpp = cinfo.output_components * 8;
        auto precision = static_cast<uint32_t>(cinfo.data_precision);
        desc.bppImage = cinfo.num_components * precision;
        desc.pitch = helpers::calculatePitch(desc.width, desc.bpp);
        desc.bitmap.resize(desc.pitch * desc.height);

        auto out = desc.bitmap.data();

        if (cinfo.data_precision == 12)
        {
#if defined(jpeg12_read_scanlines) || defined(HAVE_JPEG12)
            std::vector<uint16_t> scanline(desc.pitch);
            while (cinfo.output_scanline < cinfo.output_height && m_stop == false)
            {
                auto s = scanline.data();
                jpeg12_read_scanlines(&cinfo, (J12SAMPARRAY)&s, 1);
                for (uint32_t i = 0u; i < desc.pitch; i++)
                {
                    out[i] = static_cast<uint8_t>(scanline[i] >> 4);
                }

                out += desc.pitch;

                auto progress = static_cast<float>(cinfo.output_scanline) / cinfo.output_height;
                updateProgress(progress);
            }
#else

#endif
        }
        else if (cinfo.data_precision == 16)
        {
#if defined(jpeg16_read_scanlines) || defined(HAVE_JPEG16)
            std::vector<uint16_t> scanline(desc.pitch);
            while (cinfo.output_scanline < cinfo.output_height && m_stop == false)
            {
                auto s = scanline.data();
                jpeg16_read_scanlines(&cinfo, (J16SAMPARRAY)&s, 1);
                for (uint32_t i = 0u; i < desc.pitch; i++)
                {
                    out[i] = static_cast<uint8_t>(scanline[i] >> 8);
                }

                out += desc.pitch;

                auto progress = static_cast<float>(cinfo.output_scanline) / cinfo.output_height;
                updateProgress(progress);
            }
#else

#endif
        }
        else
        {
            while (cinfo.output_scanline < cinfo.output_height && m_stop == false)
            {
                /* jpeg_read_scanlines expects an array of pointers to scanlines.
                 * Here the array is only one element long, but you could ask for
                 * more than one scanline at a time if that's more convenient.
                 */
                jpeg_read_scanlines(&cinfo, &out, 1);
                out += desc.pitch;

                auto progress = static_cast<float>(cinfo.output_scanline) / cinfo.output_height;
                updateProgress(progress);
            }
        }
    }

    /* Step 7: Finish decompression */

    jpeg_finish_decompress(&cinfo);

    m_formatName = "jpeg";

    if (applyIccProfile(desc, icc.data(), icc.size()))
    {
        m_formatName = "jpeg/icc";
    }

    /* Step 8: Release JPEG decompression object */

    /* This is an important step since it will release a good deal of memory. */
    jpeg_destroy_decompress(&cinfo);

    /* After finish_decompress, we can close the input file.
     * Here we postpone it until after no more JPEG errors are possible,
     * so as to simplify the setjmp error logic above.  (Actually, I don't
     * think that jpeg_destroy can do an error exit, but why assume anything...)
     */

    /* At this point you may want to check to see whether any corrupt-data
     * warnings occurred (test whether jerr.pub.num_warnings is nonzero).
     */

    return true;
}

void cJpegDecoder::setupMarkers(jpeg_decompress_struct* cinfo)
{
    jpeg_save_markers(cinfo, JPEG_EXIF, MaxMarkerLength);
    jpeg_save_markers(cinfo, JPEG_ICCP, MaxMarkerLength);
}

bool cJpegDecoder::locateICCProfile(const jpeg_decompress_struct& cinfo, Icc& icc) const
{
    const char kICCPSignature[] = "ICC_PROFILE";
    const size_t kICCPSkipLength = 14; // signature + seq & count

    for (auto m = cinfo.marker_list; m != nullptr; m = m->next)
    {
        if (m->marker == cJpegDecoder::JPEG_ICCP
            && m->data_length > kICCPSkipLength
            && ::memcmp(m->data, kICCPSignature, sizeof(kICCPSignature)) == 0)
        {
            icc.resize(m->data_length - kICCPSkipLength);
            ::memcpy(icc.data(), m->data + kICCPSkipLength, icc.size());
            return true;
        }
    }

    return false;
}

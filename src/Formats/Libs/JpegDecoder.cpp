/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "JpegDecoder.h"
#include "Common/Helpers.h"

#include <cstring>
#include <jpeglib.h>
#include <setjmp.h>

namespace
{
    struct sErrorMgr
    {
        struct jpeg_error_mgr pub;
        jmp_buf setjmp_buffer;
    };

    void ErrorExit(j_common_ptr cinfo)
    {
        auto errMgr = reinterpret_cast<sErrorMgr*>(cinfo->err);
        (*cinfo->err->output_message)(cinfo);
        longjmp(errMgr->setjmp_buffer, 1);
    }

    constexpr uint32_t MaxMarkerLength = 0xffff;

} // namespace

cJpegDecoder::Result cJpegDecoder::decodeJpeg(const uint8_t* in, uint32_t size, sBitmapDescription& desc,
                                                const ProgressCallback& onProgress, const AllocatedCallback& onAllocated,
                                                const bool& stop)
{
    Result result;

    // Step 1: allocate and initialize JPEG decompression object
    jpeg_decompress_struct cinfo;
    sErrorMgr jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = ErrorExit;
    if (setjmp(jerr.setjmp_buffer))
    {
        jpeg_destroy_decompress(&cinfo);
        return result;
    }
    jpeg_create_decompress(&cinfo);

    // Step 2: specify data source
    jpeg_mem_src(&cinfo, const_cast<uint8_t*>(in), size);

    // Step 3: read file parameters with jpeg_read_header()
    setupMarkers(&cinfo);
    jpeg_read_header(&cinfo, TRUE);

    // Step 4: set parameters for decompression
    const bool isCMYK = cinfo.jpeg_color_space == JCS_CMYK || cinfo.jpeg_color_space == JCS_YCCK;
    if (isCMYK == false)
    {
        cinfo.out_color_space = JCS_RGB;
    }

    // Step 5: Start decompressor
    jpeg_start_decompress(&cinfo);

    desc.size = size;
    desc.width = cinfo.output_width;
    desc.height = cinfo.output_height;
    desc.format = ePixelFormat::RGB;

    // Extract markers before reading scanlines
    locateICCProfile(cinfo, result.iccProfile);
    locateExifData(cinfo, result.exifData);

    // Step 6: read scanlines
    if (isCMYK)
    {
        desc.bppImage = 32;
        desc.formatName = "jpeg";
        desc.allocate(desc.width, desc.height, 24, ePixelFormat::RGB);
        if (onAllocated)
        {
            onAllocated();
        }

        auto out = desc.bitmap.data();

        Buffer buffer(helpers::calculatePitch(desc.width, desc.bppImage));
        auto input = buffer.data();
        while (cinfo.output_scanline < cinfo.output_height && stop == false)
        {
            jpeg_read_scanlines(&cinfo, &input, 1);

            for (uint32_t x = 0; x < desc.width; x++)
            {
                const uint32_t src = x * 4;
                const float c = 1.0f - input[src + 0] / 255.0f;
                const float m = 1.0f - input[src + 1] / 255.0f;
                const float y = 1.0f - input[src + 2] / 255.0f;
                const float k = 1.0f - input[src + 3] / 255.0f;
                const float kInv = 1.0f - k;

                const uint32_t dst = x * 3;
                out[dst + 0] = static_cast<uint8_t>((1.0f - (c * kInv + k)) * 255.0f);
                out[dst + 1] = static_cast<uint8_t>((1.0f - (m * kInv + k)) * 255.0f);
                out[dst + 2] = static_cast<uint8_t>((1.0f - (y * kInv + k)) * 255.0f);
            }

            out += desc.pitch;

            if (onProgress)
            {
                onProgress(static_cast<float>(cinfo.output_scanline) / cinfo.output_height);
            }
        }
    }
    else
    {
        auto precision = static_cast<uint32_t>(cinfo.data_precision);
        desc.bppImage = cinfo.num_components * precision;
        auto fmt = (cinfo.output_components == 1)
            ? ePixelFormat::Luminance
            : ePixelFormat::RGB;

        desc.formatName = "jpeg";
        desc.allocate(desc.width, desc.height, cinfo.output_components * 8, fmt);
        if (onAllocated)
        {
            onAllocated();
        }

        auto out = desc.bitmap.data();

        if (cinfo.data_precision == 12)
        {
#if defined(jpeg12_read_scanlines) || defined(HAVE_JPEG12)
            std::vector<uint16_t> scanline(desc.pitch);
            while (cinfo.output_scanline < cinfo.output_height && stop == false)
            {
                auto s = scanline.data();
                jpeg12_read_scanlines(&cinfo, reinterpret_cast<J12SAMPARRAY>(&s), 1);
                for (uint32_t i = 0u; i < desc.pitch; i++)
                {
                    out[i] = static_cast<uint8_t>(scanline[i] >> 4);
                }

                out += desc.pitch;

                if (onProgress)
                {
                    onProgress(static_cast<float>(cinfo.output_scanline) / cinfo.output_height);
                }
            }
#endif
        }
        else if (cinfo.data_precision == 16)
        {
#if defined(jpeg16_read_scanlines) || defined(HAVE_JPEG16)
            std::vector<uint16_t> scanline(desc.pitch);
            while (cinfo.output_scanline < cinfo.output_height && stop == false)
            {
                auto s = scanline.data();
                jpeg16_read_scanlines(&cinfo, reinterpret_cast<J16SAMPARRAY>(&s), 1);
                for (uint32_t i = 0u; i < desc.pitch; i++)
                {
                    out[i] = static_cast<uint8_t>(scanline[i] >> 8);
                }

                out += desc.pitch;

                if (onProgress)
                {
                    onProgress(static_cast<float>(cinfo.output_scanline) / cinfo.output_height);
                }
            }
#endif
        }
        else
        {
            while (cinfo.output_scanline < cinfo.output_height && stop == false)
            {
                jpeg_read_scanlines(&cinfo, &out, 1);
                out += desc.pitch;

                if (onProgress)
                {
                    onProgress(static_cast<float>(cinfo.output_scanline) / cinfo.output_height);
                }
            }
        }
    }

    // Step 7: Finish decompression
    jpeg_finish_decompress(&cinfo);

    // Step 8: Release JPEG decompression object
    jpeg_destroy_decompress(&cinfo);

    result.success = true;
    return result;
}

cJpegDecoder::Bitmap cJpegDecoder::decodeThumbnail(const uint8_t* in, uint32_t size)
{
    Bitmap bitmap;

    jpeg_decompress_struct cinfo;
    sErrorMgr jerr;

    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = ErrorExit;
    if (setjmp(jerr.setjmp_buffer))
    {
        jpeg_destroy_decompress(&cinfo);
        return bitmap;
    }
    jpeg_create_decompress(&cinfo);

    jpeg_mem_src(&cinfo, const_cast<uint8_t*>(in), size);
    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);

    bitmap.width = cinfo.output_width;
    bitmap.height = cinfo.output_height;
    bitmap.bpp = static_cast<uint32_t>(cinfo.output_components) * 8;
    bitmap.pitch = bitmap.width * cinfo.output_components;
    bitmap.format = ePixelFormat::RGB;
    bitmap.data.resize(bitmap.pitch * bitmap.height);

    auto out = bitmap.data.data();
    while (cinfo.output_scanline < cinfo.output_height)
    {
        jpeg_read_scanlines(&cinfo, &out, 1);
        out += bitmap.pitch;
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    return bitmap;
}

void cJpegDecoder::setupMarkers(jpeg_decompress_struct* cinfo)
{
    jpeg_save_markers(cinfo, JPEG_EXIF, MaxMarkerLength);
    jpeg_save_markers(cinfo, JPEG_ICCP, MaxMarkerLength);
}

bool cJpegDecoder::locateICCProfile(const jpeg_decompress_struct& cinfo, std::vector<uint8_t>& icc)
{
    constexpr char kSignature[] = "ICC_PROFILE";
    constexpr size_t kHeaderLength = 14; // signature (12) + seq (1) + count (1)

    // First pass: count chunks and total size
    uint8_t chunkCount = 0;
    for (auto m = cinfo.marker_list; m != nullptr; m = m->next)
    {
        if (m->marker == JPEG_ICCP
            && m->data_length > kHeaderLength
            && std::memcmp(m->data, kSignature, sizeof(kSignature)) == 0)
        {
            auto count = m->data[13];
            if (chunkCount == 0)
            {
                chunkCount = count;
            }
        }
    }

    if (chunkCount == 0)
    {
        return false;
    }

    // Collect chunks by sequence number (1-based)
    struct sChunk
    {
        const uint8_t* data;
        size_t size;
    };
    std::vector<sChunk> chunks(chunkCount);
    uint8_t found = 0;

    for (auto m = cinfo.marker_list; m != nullptr; m = m->next)
    {
        if (m->marker == JPEG_ICCP
            && m->data_length > kHeaderLength
            && std::memcmp(m->data, kSignature, sizeof(kSignature)) == 0)
        {
            auto seq = m->data[12]; // 1-based sequence number
            if (seq < 1 || seq > chunkCount)
            {
                continue;
            }

            auto idx = seq - 1;
            if (chunks[idx].data == nullptr)
            {
                chunks[idx].data = m->data + kHeaderLength;
                chunks[idx].size = m->data_length - kHeaderLength;
                found++;
            }
        }
    }

    if (found != chunkCount)
    {
        return false; // Missing chunks
    }

    // Concatenate all chunks
    size_t totalSize = 0;
    for (const auto& chunk : chunks)
    {
        totalSize += chunk.size;
    }

    icc.resize(totalSize);
    size_t offset = 0;
    for (const auto& chunk : chunks)
    {
        std::memcpy(icc.data() + offset, chunk.data, chunk.size);
        offset += chunk.size;
    }

    return true;
}

bool cJpegDecoder::locateExifData(const jpeg_decompress_struct& cinfo, std::vector<uint8_t>& exif)
{
    constexpr char kExifSignature[] = "Exif";

    for (auto m = cinfo.marker_list; m != nullptr; m = m->next)
    {
        if (m->marker == JPEG_EXIF
            && m->data_length > 6
            && std::memcmp(m->data, kExifSignature, 4) == 0)
        {
            exif.assign(m->data, m->data + m->data_length);
            return true;
        }
    }

    return false;
}

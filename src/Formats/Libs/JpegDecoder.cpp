/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "JpegDecoder.h"
#include "Common/ChunkData.h"
#include "Common/Cms.h"
#include "Common/ImageInfo.h"

#include <cstring>
#include <jpeglib.h>
#include <setjmp.h>
#include <thread>

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

    // Wait until the ring buffer has room for the next row.
    // The decoder must not overwrite rows the viewer hasn't consumed yet.
    void waitForRoom(const sChunkData& chunk, uint32_t row, const bool& stop)
    {
        while (stop == false)
        {
            auto consumed = chunk.consumedHeight.load(std::memory_order_acquire);
            if (row - consumed < chunk.bandHeight)
            {
                break;
            }
            std::this_thread::yield();
        }
    }

    void emitRow(sChunkData& chunk, uint32_t row,
                 const cJpegDecoder::ProgressCallback& onProgress, float progressBase, float progressScale)
    {
        chunk.readyHeight.store(row + 1, std::memory_order_release);

        if (onProgress)
        {
            auto p = static_cast<float>(row + 1) / chunk.height;
            onProgress(progressBase + p * progressScale);
        }
    }

    void readScanlines(jpeg_decompress_struct& cinfo, sChunkData& chunk,
                       const bool& stop,
                       const cJpegDecoder::ProgressCallback& onProgress, float progressBase, float progressScale)
    {
        if (cinfo.data_precision == 12)
        {
#if defined(HAVE_JPEG12)
            std::vector<uint16_t> scanline(chunk.pitch);
            while (cinfo.output_scanline < cinfo.output_height && stop == false)
            {
                const uint32_t row = cinfo.output_scanline;
                waitForRoom(chunk, row, stop);
                if (stop)
                {
                    break;
                }

                auto s = scanline.data();
                jpeg12_read_scanlines(&cinfo, reinterpret_cast<J12SAMPARRAY>(&s), 1);
                auto out = chunk.rowPtr(row);
                for (uint32_t i = 0u; i < chunk.pitch; i++)
                {
                    out[i] = static_cast<uint8_t>(scanline[i] >> 4);
                }

                emitRow(chunk, row, onProgress, progressBase, progressScale);
            }
#endif
        }
        else if (cinfo.data_precision == 16)
        {
#if defined(HAVE_JPEG16)
            std::vector<uint16_t> scanline(chunk.pitch);
            while (cinfo.output_scanline < cinfo.output_height && stop == false)
            {
                const uint32_t row = cinfo.output_scanline;
                waitForRoom(chunk, row, stop);
                if (stop)
                {
                    break;
                }

                auto s = scanline.data();
                jpeg16_read_scanlines(&cinfo, reinterpret_cast<J16SAMPARRAY>(&s), 1);
                auto out = chunk.rowPtr(row);
                for (uint32_t i = 0u; i < chunk.pitch; i++)
                {
                    out[i] = static_cast<uint8_t>(scanline[i] >> 8);
                }

                emitRow(chunk, row, onProgress, progressBase, progressScale);
            }
#endif
        }
        else
        {
            while (cinfo.output_scanline < cinfo.output_height && stop == false)
            {
                const uint32_t row = cinfo.output_scanline;
                waitForRoom(chunk, row, stop);
                if (stop)
                {
                    break;
                }

                auto out = chunk.rowPtr(row);
                jpeg_read_scanlines(&cinfo, &out, 1);

                emitRow(chunk, row, onProgress, progressBase, progressScale);
            }
        }
    }

} // namespace

cJpegDecoder::Result cJpegDecoder::decodeJpeg(const uint8_t* in, uint32_t size, sChunkData& chunk, sImageInfo& info,
                                              const ProgressCallback& onProgress, const AllocatedCallback& onAllocated,
                                              const ImageInfoCallback& onImageInfo, const PreviewCallback& onPreview,
                                              const bool& stop)
{
    Result result;

    // Step 1: allocate and initialize JPEG decompression object
    jpeg_decompress_struct cinfo;
    sErrorMgr jerr;

    cinfo.err           = jpeg_std_error(&jerr.pub);
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

    // Compute output dimensions early so we can signal image info
    // before decompression starts.
    jpeg_calc_output_dimensions(&cinfo);

    info.fileSize = size;
    chunk.width   = cinfo.output_width;
    chunk.height  = cinfo.output_height;
    info.bppImage = cinfo.num_components * static_cast<uint32_t>(cinfo.data_precision);

    // Extract markers (available after jpeg_read_header)
    locateICCProfile(cinfo, result.iccProfile);
    locateExifData(cinfo, result.exifData);

    // Set formatName based on ICC presence so the viewer shows
    // the correct type from the start.
    const bool hasIcc = result.iccProfile.empty() == false;
    info.formatName   = hasIcc
        ? "jpeg/icc"
        : "jpeg";

    if (onImageInfo)
    {
        onImageInfo();
    }

    // Extract EXIF thumbnail as preview before expensive decode
    if (onPreview && result.exifData.empty() == false)
    {
        const uint8_t* thumbData = nullptr;
        uint32_t thumbSize       = 0;
        if (locateExifThumbnail(result.exifData, thumbData, thumbSize))
        {
            auto thumb = decodeThumbnail(thumbData, thumbSize);
            if (thumb.width > 0 && thumb.height > 0)
            {
                onPreview(std::move(thumb));
            }
        }
    }

    // Step 5: Start decompressor
    // For progressive JPEGs, jpeg_start_decompress performs all internal
    // multi-pass decoding. After that, jpeg_read_scanlines works identically
    // to baseline — one row at a time with stop checks.
    jpeg_start_decompress(&cinfo);

    // Step 6: allocate bitmap as a band buffer
    constexpr uint32_t BandRows = 8192;
    ePixelFormat fmt;
    if (isCMYK)
    {
        // Upload raw CMYK bytes — GPU shader converts to RGB
        fmt = ePixelFormat::RGBA;
        chunk.effects |= eEffect::Cmyk;
        chunk.allocate(chunk.width, chunk.height, 32, fmt, BandRows);
    }
    else
    {
        fmt = (cinfo.output_components == 1)
            ? ePixelFormat::Luminance
            : ePixelFormat::RGB;
        chunk.allocate(chunk.width, chunk.height, cinfo.output_components * 8, fmt, BandRows);
    }

    // Step 7: generate 3D LUT from ICC profile (applied on GPU during rendering)
    if (result.iccProfile.empty() == false)
    {
        chunk.lutData = cms::generateLut3D(
            result.iccProfile.data(), static_cast<uint32_t>(result.iccProfile.size()), fmt);
        if (chunk.lutData.empty() == false)
        {
            chunk.effects |= eEffect::Lut;
        }
    }

    if (onAllocated)
    {
        onAllocated();
    }

    // Step 8: read scanlines into ring buffer (no CPU transforms)
    readScanlines(cinfo, chunk, stop, onProgress, 0.0f, 1.0f);

    // Step 9: Finish decompression
    jpeg_finish_decompress(&cinfo);

    // Step 10: Release JPEG decompression object
    jpeg_destroy_decompress(&cinfo);

    result.success = true;
    return result;
}

cJpegDecoder::Bitmap cJpegDecoder::decodeThumbnail(const uint8_t* in, uint32_t size)
{
    Bitmap bitmap;

    jpeg_decompress_struct cinfo;
    sErrorMgr jerr;

    cinfo.err           = jpeg_std_error(&jerr.pub);
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

    bitmap.width  = cinfo.output_width;
    bitmap.height = cinfo.output_height;
    bitmap.bpp    = static_cast<uint32_t>(cinfo.output_components) * 8;
    bitmap.pitch  = bitmap.width * cinfo.output_components;
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
    constexpr char kSignature[]    = "ICC_PROFILE";
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

bool cJpegDecoder::locateExifThumbnail(const std::vector<uint8_t>& exif, const uint8_t*& jpegData, uint32_t& jpegSize)
{
    // EXIF layout: "Exif\0\0" (6 bytes) + TIFF header + IFD chain
    // IFD1 contains thumbnail: tag 0x0201 = offset, tag 0x0202 = length
    constexpr size_t kExifHeaderSize = 6;
    constexpr size_t kTiffHeaderSize = 8;

    if (exif.size() < kExifHeaderSize + kTiffHeaderSize)
    {
        return false;
    }

    const auto tiff      = exif.data() + kExifHeaderSize;
    const size_t tiffLen = exif.size() - kExifHeaderSize;

    // Determine byte order
    bool bigEndian;
    if (tiff[0] == 'M' && tiff[1] == 'M')
    {
        bigEndian = true;
    }
    else if (tiff[0] == 'I' && tiff[1] == 'I')
    {
        bigEndian = false;
    }
    else
    {
        return false;
    }

    auto read16 = [bigEndian](const uint8_t* p) -> uint16_t {
        return bigEndian
            ? static_cast<uint16_t>((p[0] << 8) | p[1])
            : static_cast<uint16_t>((p[1] << 8) | p[0]);
    };

    auto read32 = [bigEndian](const uint8_t* p) -> uint32_t {
        return bigEndian
            ? (static_cast<uint32_t>(p[0]) << 24) | (static_cast<uint32_t>(p[1]) << 16) | (static_cast<uint32_t>(p[2]) << 8) | p[3]
            : (static_cast<uint32_t>(p[3]) << 24) | (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[1]) << 8) | p[0];
    };

    // Verify TIFF magic
    if (read16(tiff + 2) != 42)
    {
        return false;
    }

    // Navigate to IFD0
    uint32_t ifdOffset = read32(tiff + 4);
    if (ifdOffset + 2 > tiffLen)
    {
        return false;
    }

    // Skip IFD0 to find IFD1
    uint16_t entryCount         = read16(tiff + ifdOffset);
    constexpr size_t kEntrySize = 12;
    uint32_t nextIfdPtr         = ifdOffset + 2 + entryCount * kEntrySize;
    if (nextIfdPtr + 4 > tiffLen)
    {
        return false;
    }

    uint32_t ifd1Offset = read32(tiff + nextIfdPtr);
    if (ifd1Offset == 0 || ifd1Offset + 2 > tiffLen)
    {
        return false;
    }

    // Parse IFD1 for thumbnail offset and length
    uint16_t ifd1Count   = read16(tiff + ifd1Offset);
    uint32_t thumbOffset = 0;
    uint32_t thumbLength = 0;

    for (uint16_t i = 0; i < ifd1Count; i++)
    {
        size_t entryPos = ifd1Offset + 2 + i * kEntrySize;
        if (entryPos + kEntrySize > tiffLen)
        {
            break;
        }

        uint16_t tag = read16(tiff + entryPos);
        if (tag == 0x0201) // JPEGInterchangeFormat
        {
            thumbOffset = read32(tiff + entryPos + 8);
        }
        else if (tag == 0x0202) // JPEGInterchangeFormatLength
        {
            thumbLength = read32(tiff + entryPos + 8);
        }
    }

    if (thumbOffset == 0 || thumbLength == 0)
    {
        return false;
    }

    // Offsets are relative to TIFF header start
    if (static_cast<size_t>(thumbOffset) + thumbLength > tiffLen)
    {
        return false;
    }

    jpegData = tiff + thumbOffset;
    jpegSize = thumbLength;
    return true;
}

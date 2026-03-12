/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatJp2k.h"

#if defined(JPEG2000_SUPPORT)

#include "Common/Callbacks.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
#include "Log/Log.h"

#include <cstring>
#include <openjpeg.h>
#include <thread>

namespace
{
    void j2k_info_callback(const char* /*msg*/, void* /*client_data*/)
    {
    }

    void j2k_warning_callback(const char* /*msg*/, void* /*client_data*/)
    {
    }

    void j2k_error_callback(const char* msg, void* client_data)
    {
        auto stop = static_cast<const bool*>(client_data);
        if (stop == nullptr || *stop == false)
        {
            std::string_view sv(msg);
            while (sv.empty() == false && (sv.back() == '\n' || sv.back() == '\r'))
            {
                sv.remove_suffix(1);
            }
            cLog::Error("{}", sv);
        }
    }

    const char* getColorSpaceName(COLOR_SPACE type)
    {
        switch (type)
        {
        case OPJ_CLRSPC_UNKNOWN:
            return "Unknown";

        case OPJ_CLRSPC_UNSPECIFIED:
            return "Unspecified";

        case OPJ_CLRSPC_SRGB:
            return "sRGB";

        case OPJ_CLRSPC_GRAY:
            return "GRAYSCALE";

        case OPJ_CLRSPC_SYCC:
            return "SYCC";

        case OPJ_CLRSPC_EYCC:
            return "EYCC";

        case OPJ_CLRSPC_CMYK:
            return "CMYK";
        }

        return "Unknown";
    }

    struct StreamContext
    {
        cFile* file;
        const bool* stop;
    };

    size_t streamRead(void* buffer, size_t size, void* user)
    {
        auto ctx = static_cast<StreamContext*>(user);
        if (*ctx->stop)
        {
            return static_cast<size_t>(-1);
        }
        auto bytesRead = ctx->file->read(buffer, size);
        return bytesRead
            ? bytesRead
            : static_cast<size_t>(-1);
    }

    void streamClose(void* /*user*/)
    {
        // File is owned by LoadImpl's stack frame — let cFile destructor handle closing.
    }

    off_t streamSkip(off_t bytes, void* user)
    {
        auto ctx = static_cast<StreamContext*>(user);
        if (*ctx->stop)
        {
            return -1;
        }
        if (ctx->file->seek(bytes, SEEK_CUR) != 0)
        {
            return -1;
        }

        return bytes;
    }

    int streamSeek(off_t bytes, void* user)
    {
        auto ctx = static_cast<StreamContext*>(user);
        if (*ctx->stop)
        {
            return 0;
        }
        return ctx->file->seek(bytes, SEEK_SET) == 0;
    }

    bool determinePixelFormat(opj_image_t* image, uint32_t& outBpp, ePixelFormat& outFormat)
    {
        auto colorspace = image->color_space;
        if (colorspace == OPJ_CLRSPC_SYCC || colorspace == OPJ_CLRSPC_EYCC)
        {
            return false;
        }

        uint32_t numcomps = image->numcomps;

        for (uint32_t c = 0; c < numcomps - 1; c++)
        {
            if (image->comps[c].dx != image->comps[c + 1].dx
                || image->comps[c].dy != image->comps[c + 1].dy
                || image->comps[c].prec != image->comps[c + 1].prec)
            {
                return false;
            }
        }

        if (image->comps[0].prec > 16)
        {
            return false;
        }

        if (numcomps == 1)
        {
            outBpp    = 8;
            outFormat = ePixelFormat::Luminance;
        }
        else if (numcomps == 2)
        {
            outBpp    = 16;
            outFormat = ePixelFormat::LuminanceAlpha;
        }
        else if (numcomps == 3)
        {
            outBpp    = 24;
            outFormat = ePixelFormat::RGB;
        }
        else if (colorspace == OPJ_CLRSPC_CMYK)
        {
            outBpp    = 32;
            outFormat = ePixelFormat::RGBA;
        }
        else
        {
            outBpp    = 32;
            outFormat = ePixelFormat::RGBA;
        }

        return true;
    }

    struct CodecContext
    {
        opj_codec_t* codec = nullptr;
        opj_image_t* image = nullptr;

        CodecContext() = default;
        ~CodecContext()
        {
            if (codec != nullptr)
            {
                opj_destroy_codec(codec);
            }
            if (image != nullptr)
            {
                opj_image_destroy(image);
            }
        }

        CodecContext(const CodecContext&)            = delete;
        CodecContext& operator=(const CodecContext&) = delete;
        CodecContext(CodecContext&& o) noexcept
            : codec(o.codec)
            , image(o.image)
        {
            o.codec = nullptr;
            o.image = nullptr;
        }
        CodecContext& operator=(CodecContext&& o) noexcept
        {
            if (this != &o)
            {
                if (codec != nullptr)
                {
                    opj_destroy_codec(codec);
                }
                if (image != nullptr)
                {
                    opj_image_destroy(image);
                }
                codec   = o.codec;
                image   = o.image;
                o.codec = nullptr;
                o.image = nullptr;
            }
            return *this;
        }
    };

    opj_stream_t* createStream(StreamContext* ctx, long fileSize)
    {
        auto stream = opj_stream_default_create(true);
        opj_stream_set_user_data(stream, ctx, streamClose);
        opj_stream_set_user_data_length(stream, fileSize);
        opj_stream_set_read_function(stream, streamRead);
        opj_stream_set_skip_function(stream, streamSkip);
        opj_stream_set_seek_function(stream, streamSeek);
        return stream;
    }

    bool createCodec(CodecContext& ctx, opj_stream_t* stream, const bool* stopFlag, uint32_t reduceFactor)
    {
        ctx.codec = opj_create_decompress(OPJ_CODEC_JP2);

        opj_set_info_handler(ctx.codec, j2k_info_callback, nullptr);
        opj_set_warning_handler(ctx.codec, j2k_warning_callback, nullptr);
        opj_set_error_handler(ctx.codec, j2k_error_callback, const_cast<bool*>(stopFlag));

        opj_dparameters_t parameters;
        opj_set_default_decoder_parameters(&parameters);
        parameters.cp_reduce = reduceFactor;

        if (opj_setup_decoder(ctx.codec, &parameters) == false)
        {
            return false;
        }

        const auto numThreads = std::max(1u, std::thread::hardware_concurrency());
        opj_codec_set_threads(ctx.codec, static_cast<int>(numThreads));

        if (opj_read_header(stream, ctx.codec, &ctx.image) == false)
        {
            return false;
        }

        return true;
    }

} // namespace

bool cFormatJp2k::isSupported(cFile& file, Buffer& buffer) const
{
    const uint8_t jp2_signature[] = { 0x00, 0x00, 0x00, 0x0C, 0x6A, 0x50, 0x20, 0x20, 0x0D, 0x0A, 0x87, 0x0A };

    if (!readBuffer(file, buffer, sizeof(jp2_signature)))
    {
        return false;
    }

    return ::memcmp(jp2_signature, buffer.data(), sizeof(jp2_signature)) == 0;
}

bool cFormatJp2k::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (!openFile(file, filename, info))
    {
        return false;
    }

    StreamContext sctx{ &file, &m_stop };

    // Phase 1: Read header to get tile/resolution info and determine preview factor.
    auto stream = createStream(&sctx, info.fileSize);
    CodecContext headerCtx;
    if (createCodec(headerCtx, stream, &m_stop, 0) == false)
    {
        cLog::Error("Can't read JPEG2000 header.");
        opj_stream_destroy(stream);
        return false;
    }

    auto cstrInfo                 = opj_get_cstr_info(headerCtx.codec);
    const uint32_t numTilesX      = cstrInfo->tw;
    const uint32_t numTilesY      = cstrInfo->th;
    const uint32_t tdx            = cstrInfo->tdx;
    const uint32_t tdy            = cstrInfo->tdy;
    const uint32_t numTiles       = numTilesX * numTilesY;
    const uint32_t numResolutions = cstrInfo->m_default_tile_info.tccp_info[0].numresolutions;
    opj_destroy_cstr_info(&cstrInfo);

    const uint32_t fullWidth  = headerCtx.image->comps[0].w;
    const uint32_t fullHeight = headerCtx.image->comps[0].h;

    cLog::Debug("Tile grid: {}x{} ({}x{} per tile), {} total, {} resolutions.",
                numTilesX, numTilesY, tdx, tdy, numTiles, numResolutions);

    // Determine if a reduced-resolution preview is worthwhile.
    const uint32_t maxDim            = std::max(fullWidth, fullHeight);
    constexpr uint32_t PreviewTarget = 2000;
    uint32_t reduceFactor            = 0;
    if (numResolutions > 1)
    {
        while (reduceFactor + 1 < numResolutions
               && (maxDim >> (reduceFactor + 1)) >= PreviewTarget)
        {
            reduceFactor++;
        }
    }

    // Done with header-only codec.
    headerCtx = {};
    opj_stream_destroy(stream);
    stream = nullptr;

    // Phase 2: Quick low-resolution preview (if image is large enough).
    if (reduceFactor > 0 && m_stop == false)
    {
        decodePreview(file, info.fileSize, reduceFactor, fullWidth, fullHeight);
    }

    if (m_stop)
    {
        return false;
    }

    // Phase 3: Full-resolution decode.
    file.seek(0, SEEK_SET);
    stream = createStream(&sctx, info.fileSize);
    CodecContext fullCtx;
    if (createCodec(fullCtx, stream, &m_stop, 0) == false)
    {
        cLog::Error("Can't set up JPEG2000 full-res decoder.");
        opj_stream_destroy(stream);
        return false;
    }

    // Determine pixel format and allocate bitmap.
    uint32_t bpp;
    ePixelFormat format;
    if (determinePixelFormat(fullCtx.image, bpp, format) == false)
    {
        cLog::Error("Unsupported JPEG2000 format.");
        opj_stream_destroy(stream);
        return false;
    }

    const uint32_t numcomps = fullCtx.image->numcomps;
    chunk.width             = fullCtx.image->comps[0].w;
    chunk.height            = fullCtx.image->comps[0].h;
    info.bppImage           = numcomps * fullCtx.image->comps[0].prec;
    info.images             = 1;

    cLog::Debug("Components: {}.", numcomps);
    cLog::Debug("  Colorspace: {}.", getColorSpaceName(fullCtx.image->color_space));
    cLog::Debug("  Prec: {}.", fullCtx.image->comps[0].prec);
    cLog::Debug("  Signed: {}.", fullCtx.image->comps[0].sgnd);
    cLog::Debug("  Factor: {}.", fullCtx.image->comps[0].factor);
    cLog::Debug("  Decoded resolution: {}.", fullCtx.image->comps[0].resno_decoded);

    const bool hasIcc = fullCtx.image->icc_profile_buf != nullptr
        && fullCtx.image->icc_profile_len > 0;
    info.formatName = hasIcc
        ? "jpeg2000/icc"
        : "jpeg2000";

    chunk.allocate(chunk.width, chunk.height, bpp, format);
    if (fullCtx.image->color_space == OPJ_CLRSPC_CMYK)
    {
        chunk.effects |= eEffect::Cmyk;
    }

    // Generate ICC LUT before signalBitmapAllocated so it's correct from the first frame.
    if (hasIcc)
    {
        applyIccProfile(chunk, fullCtx.image->icc_profile_buf, fullCtx.image->icc_profile_len);
    }

    signalBitmapAllocated();

    // Decode pixels.
    bool decodeOk = true;

    if (numTiles == 1)
    {
        // Single-tile image: strip-based decoding via opj_set_decode_area().
        constexpr uint32_t StripHeight = 4096;
        const uint32_t numStrips       = (chunk.height + StripHeight - 1) / StripHeight;

        for (uint32_t strip = 0; strip < numStrips && m_stop == false; strip++)
        {
            const uint32_t y0 = strip * StripHeight;
            const uint32_t y1 = std::min(y0 + StripHeight, chunk.height);

            if (opj_set_decode_area(fullCtx.codec, fullCtx.image, 0, y0, chunk.width, y1) == false)
            {
                if (m_stop == false)
                {
                    cLog::Error("Can't set JPEG2000 decode area for strip {}.", strip);
                }
                decodeOk = false;
                break;
            }

            if (opj_decode(fullCtx.codec, stream, fullCtx.image) == false)
            {
                if (m_stop == false)
                {
                    cLog::Error("Can't decode JPEG2000 strip {}/{}.", strip + 1, numStrips);
                }
                decodeOk = false;
                break;
            }

            convertPixels(fullCtx.image, chunk, 0, y0);

            chunk.readyHeight.store(y1, std::memory_order_release);
            updateProgress(static_cast<float>(strip + 1) / numStrips);
        }
    }
    else
    {
        // Multi-tile image: decode tile-by-tile.
        for (uint32_t tileIdx = 0; tileIdx < numTiles && m_stop == false; tileIdx++)
        {
            if (opj_get_decoded_tile(fullCtx.codec, stream, fullCtx.image, tileIdx) == false)
            {
                if (m_stop == false)
                {
                    cLog::Error("Can't decode JPEG2000 tile {}/{}.", tileIdx + 1, numTiles);
                }
                decodeOk = false;
                break;
            }

            const uint32_t tileX = tileIdx % numTilesX;
            const uint32_t tileY = tileIdx / numTilesX;

            convertPixels(fullCtx.image, chunk, tileX * tdx, tileY * tdy);

            if (tileX == numTilesX - 1)
            {
                const uint32_t ready = std::min((tileY + 1) * tdy, chunk.height);
                chunk.readyHeight.store(ready, std::memory_order_release);
            }

            updateProgress(static_cast<float>(tileIdx + 1) / numTiles);
        }
    }

    if (m_stop || decodeOk == false)
    {
        opj_stream_destroy(stream);
        return false;
    }

    chunk.readyHeight.store(chunk.height, std::memory_order_release);

    if (opj_end_decompress(fullCtx.codec, stream) == false)
    {
        cLog::Error("Can't finalize JPEG2000 decompression.");
    }

    opj_stream_destroy(stream);

    return true;
}

void cFormatJp2k::decodePreview(cFile& file, long fileSize, uint32_t reduceFactor, uint32_t fullWidth, uint32_t fullHeight)
{
    file.seek(0, SEEK_SET);

    StreamContext sctx{ &file, &m_stop };
    auto stream = createStream(&sctx, fileSize);

    CodecContext ctx;
    if (createCodec(ctx, stream, &m_stop, reduceFactor) == false)
    {
        opj_stream_destroy(stream);
        return;
    }

    if (opj_decode(ctx.codec, stream, ctx.image) == false || m_stop)
    {
        opj_stream_destroy(stream);
        return;
    }

    opj_stream_destroy(stream);

    uint32_t bpp;
    ePixelFormat format;
    if (determinePixelFormat(ctx.image, bpp, format) == false)
    {
        return;
    }

    // Use a temporary sChunkData for convertPixels.
    sChunkData previewChunk;
    previewChunk.allocate(ctx.image->comps[0].w, ctx.image->comps[0].h, bpp, format);

    convertPixels(ctx.image, previewChunk, 0, 0);
    if (m_stop)
    {
        return;
    }

    sPreviewData preview;
    preview.width           = previewChunk.width;
    preview.height          = previewChunk.height;
    preview.pitch           = previewChunk.pitch;
    preview.bpp             = bpp;
    preview.format          = format;
    preview.fullImageWidth  = fullWidth;
    preview.fullImageHeight = fullHeight;
    preview.bitmap          = std::move(previewChunk.bitmap);

    signalPreviewReady(std::move(preview));
}

void cFormatJp2k::convertPixels(opj_image_t* image, sChunkData& chunk, uint32_t dstX, uint32_t dstY)
{
    const uint32_t tileW         = image->comps[0].w;
    const uint32_t tileH         = image->comps[0].h;
    const uint32_t numcomps      = image->numcomps;
    const uint32_t bytesPerPixel = chunk.bpp / 8;
    const uint32_t prec          = image->comps[0].prec;
    const uint32_t shift         = prec > 8
        ? (prec - 8)
        : 0u;
    const bool sgnd              = image->comps[0].sgnd != 0;

    // Read a component sample and normalize to 8-bit.
    auto read8 = [sgnd, shift](const opj_image_comp_t& comp, uint32_t pos) -> uint8_t {
        auto value = static_cast<uint32_t>(comp.data[pos]);
        value += sgnd
            ? (1u << (comp.prec - 1))
            : 0u;
        return static_cast<uint8_t>(value >> shift);
    };

    // Branch on numcomps outside the loops so the compiler can inline read8.
    auto* comps = image->comps;

    auto packRow = [&](uint32_t y, auto packPixel) {
        auto bits = chunk.bitmap.data() + chunk.pitch * (dstY + y) + dstX * bytesPerPixel;
        for (uint32_t x = 0; x < tileW; x++)
        {
            const uint32_t pos = y * tileW + x;
            packPixel(pos, bits);
        }
    };

    switch (numcomps)
    {
    case 1:
        for (uint32_t y = 0; y < tileH && m_stop == false; y++)
        {
            packRow(y, [&](uint32_t pos, uint8_t*& bits) {
                *bits++ = read8(comps[0], pos);
            });
        }
        break;

    case 2:
        for (uint32_t y = 0; y < tileH && m_stop == false; y++)
        {
            packRow(y, [&](uint32_t pos, uint8_t*& bits) {
                *bits++ = read8(comps[0], pos);
                *bits++ = read8(comps[1], pos);
            });
        }
        break;

    case 3:
        for (uint32_t y = 0; y < tileH && m_stop == false; y++)
        {
            packRow(y, [&](uint32_t pos, uint8_t*& bits) {
                *bits++ = read8(comps[0], pos);
                *bits++ = read8(comps[1], pos);
                *bits++ = read8(comps[2], pos);
            });
        }
        break;

    default:
        for (uint32_t y = 0; y < tileH && m_stop == false; y++)
        {
            packRow(y, [&](uint32_t pos, uint8_t*& bits) {
                *bits++ = read8(comps[0], pos);
                *bits++ = read8(comps[1], pos);
                *bits++ = read8(comps[2], pos);
                *bits++ = read8(comps[3], pos);
            });
        }
        break;
    }
}

#endif

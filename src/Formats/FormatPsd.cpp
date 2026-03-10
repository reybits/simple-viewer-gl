/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatPsd.h"
#include "Common/Callbacks.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/Helpers.h"
#include "Common/ImageInfo.h"
#include "Libs/JpegDecoder.h"
#include "Log/Log.h"

#include <cstring>
#include <iterator>
#include <vector>

namespace
{
    // http://www.adobe.com/devnet-apps/photoshop/fileformatashtml/
    enum class ColorMode : uint16_t
    {
        MONO = 0,
        GRAYSCALE = 1,
        INDEXED = 2,
        RGB = 3,
        CMYK = 4,
        // UNUSED    = 5,
        // UNUSED    = 6,
        MULTICHANNEL = 7,
        DUOTONE = 8,
        LAB = 9
    };

#pragma pack(push, 1)
    struct PSD_HEADER
    {
        uint8_t signature[4]; // file ID, always "8BPS"
        uint16_t version;     // version number, always 1
        uint8_t reserved[6];
        uint16_t channels;   // number of color channels (1-56)
        uint32_t rows;       // height of image in pixels (1-30000)
        uint32_t columns;    // width of image in pixels (1-30000)
        uint16_t depth;      // number of bits per channel (1, 8, 16, 32)
        ColorMode colorMode; // color mode as defined below
    };
#pragma pack(pop)

    const char* modeToString(ColorMode colorMode)
    {
        static const char* modes[] = {
            "MONO",
            "GRAYSCALE",
            "INDEXED",
            "RGB",
            "CMYK",
            "unknown",
            "unknown",
            "MULTICHANNEL",
            "DUOTONE",
            "LAB",
            "unknown"
        };
        static const size_t size = std::size(modes);

        if (static_cast<uint32_t>(colorMode) < size)
        {
            return modes[static_cast<uint32_t>(colorMode)];
        }
        return modes[size - 1];
    }

    enum class CompressionMethod : uint16_t
    {
        RAW = 0,        // Raw image data
        RLE = 1,        // RLE compressed the image data starts with the byte counts
                        // for all the scan lines (rows * channels), with each count
                        // stored as a two-byte value. The RLE compressed data follows,
                        // with each scan line compressed separately. The RLE compression
                        // is the same compression algorithm used by the Macintosh ROM
                        // routine PackBits, and the TIFF standard.
        ZIP = 2,        // ZIP without prediction
        ZIP_PREDICT = 3 // ZIP with prediction
    };

    bool skipNextBlock(cFile& file)
    {
        uint32_t size;
        if (sizeof(uint32_t) != file.read(&size, sizeof(uint32_t)))
        {
            return false;
        }
        size = helpers::read_uint32(reinterpret_cast<uint8_t*>(&size));
        file.seek(size, SEEK_CUR);

        return true;
    }

    constexpr uint16_t PSD_THUMBNAIL_RESOURCE = 0x040C;
    constexpr uint16_t PSD_ICC_PROFILE_RESOURCE = 0x040F;

    // PSD thumbnail resource header (28 bytes before JPEG data)
    struct ThumbnailHeader
    {
        uint32_t format; // 1 = kJpegRGB
        uint32_t width;
        uint32_t height;
        uint32_t widthBytes;
        uint32_t totalSize;
        uint32_t compressedSize;
        uint16_t bitsPerPixel;
        uint16_t numPlanes;
    };

    // Read Image Resources Block and extract ICC profile and thumbnail if present.
    // Returns false on read error.
    bool readImageResources(cFile& file, Buffer& iccProfile, Buffer& thumbnailJpeg)
    {
        uint32_t blockSize;
        if (sizeof(uint32_t) != file.read(&blockSize, sizeof(uint32_t)))
        {
            return false;
        }
        blockSize = helpers::read_uint32(reinterpret_cast<uint8_t*>(&blockSize));

        auto blockEnd = file.getOffset() + blockSize;

        if (blockEnd > file.getSize())
        {
            return false;
        }

        while (file.getOffset() < blockEnd)
        {
            // Resource entry: "8BIM" signature
            uint8_t sig[4];
            if (4 != file.read(sig, 4))
            {
                break;
            }
            if (sig[0] != '8' || sig[1] != 'B' || sig[2] != 'I' || sig[3] != 'M')
            {
                break;
            }

            // Resource ID (big-endian)
            uint16_t resourceId;
            if (sizeof(uint16_t) != file.read(&resourceId, sizeof(uint16_t)))
            {
                break;
            }
            resourceId = helpers::read_uint16(reinterpret_cast<uint8_t*>(&resourceId));

            // Pascal string name (1 byte length + chars, padded to even total)
            uint8_t nameLen;
            if (1 != file.read(&nameLen, 1))
            {
                break;
            }
            // Total name field size is padded to even (including the length byte)
            uint32_t nameFieldSize = (nameLen + 2) & ~1u;
            file.seek(nameFieldSize - 1, SEEK_CUR);

            // Resource data size (big-endian)
            uint32_t dataSize;
            if (sizeof(uint32_t) != file.read(&dataSize, sizeof(uint32_t)))
            {
                break;
            }
            dataSize = helpers::read_uint32(reinterpret_cast<uint8_t*>(&dataSize));

            if (resourceId == PSD_ICC_PROFILE_RESOURCE && dataSize > 0)
            {
                iccProfile.resize(dataSize);
                if (dataSize != file.read(iccProfile.data(), dataSize))
                {
                    iccProfile.clear();
                }
            }
            else if (resourceId == PSD_THUMBNAIL_RESOURCE && dataSize > sizeof(ThumbnailHeader))
            {
                ThumbnailHeader th;
                if (sizeof(ThumbnailHeader) != file.read(&th, sizeof(ThumbnailHeader)))
                {
                    file.seek(dataSize - sizeof(ThumbnailHeader), SEEK_CUR);
                }
                else
                {
                    auto format = helpers::read_uint32(reinterpret_cast<uint8_t*>(&th.format));
                    auto jpegSize = helpers::read_uint32(reinterpret_cast<uint8_t*>(&th.compressedSize));
                    if (format == 1 && jpegSize > 0 && jpegSize <= dataSize - sizeof(ThumbnailHeader))
                    {
                        thumbnailJpeg.resize(jpegSize);
                        if (jpegSize != file.read(thumbnailJpeg.data(), jpegSize))
                        {
                            thumbnailJpeg.clear();
                        }
                        // Skip remaining bytes if any
                        auto remaining = dataSize - sizeof(ThumbnailHeader) - jpegSize;
                        if (remaining > 0)
                        {
                            file.seek(remaining, SEEK_CUR);
                        }
                    }
                    else
                    {
                        file.seek(dataSize - sizeof(ThumbnailHeader), SEEK_CUR);
                    }
                }
            }
            else
            {
                // Skip resource data (padded to even size)
                file.seek((dataSize + 1) & ~1u, SEEK_CUR);
                continue;
            }

            // Data is padded to even size
            if (dataSize & 1)
            {
                file.seek(1, SEEK_CUR);
            }
        }

        // Ensure we're positioned at the end of the block
        file.seek(blockEnd, SEEK_SET);
        return true;
    }

    void decodeRle(uint8_t* dst, const uint8_t* src, uint32_t lineLength)
    {
        uint16_t bytesRead = 0;
        while (bytesRead < lineLength)
        {
            const auto byte = static_cast<signed char>(src[bytesRead]);
            bytesRead++;

            if (byte == -128)
            {
                continue;
            }
            else if (byte > -1)
            {
                const int count = byte + 1;

                // copy next count bytes
                for (int i = 0; i < count; i++)
                {
                    *dst = src[bytesRead];
                    dst++;
                    bytesRead++;
                }
            }
            else
            {
                const int count = -byte + 1;

                // copy next byte count times
                const uint8_t next_byte = src[bytesRead];
                bytesRead++;
                for (int i = 0; i < count; i++)
                {
                    *dst = next_byte;
                    dst++;
                }
            }
        }
    }

    // Interleave planar channels into packed pixel layout.
    // Handles 8/16/32-bit depth by reading the MSB of big-endian values.
    void interleaveChannels(sChunkData& chunk, const std::vector<std::vector<uint8_t>>& chBufs,
                            uint32_t numChannels, uint32_t bytesPerComponent)
    {
        for (uint32_t y = 0; y < chunk.height; y++)
        {
            auto out = chunk.bitmap.data() + y * chunk.pitch;
            for (uint32_t x = 0; x < chunk.width; x++)
            {
                const uint32_t idx = (chunk.width * y + x) * bytesPerComponent;
                for (uint32_t ch = 0; ch < numChannels; ch++)
                {
                    // For >8-bit depth, take MSB of big-endian value
                    out[ch] = chBufs[ch][idx];
                }
                out += numChannels;
            }
        }
    }

    // Downscale 16-bit or 32-bit planar channel to 8-bit.
    // For 16-bit big-endian: shift right by 8. For 32-bit big-endian float: read byte [1].
    template <typename C>
    void fromRgba(sChunkData& chunk, const C* r, const C* g, const C* b, const C* a)
    {
        const auto shift = static_cast<uint32_t>(sizeof(C)) >> 1;
        for (uint32_t y = 0; y < chunk.height; y++)
        {
            uint32_t idx = chunk.width * y;
            auto out = chunk.bitmap.data() + y * chunk.pitch;
            for (uint32_t x = 0; x < chunk.width; x++)
            {
                out[0] = r[idx] >> shift;
                out[1] = g[idx] >> shift;
                out[2] = b[idx] >> shift;
                out[3] = a[idx] >> shift;
                out += 4;
                idx++;
            }
        }
    }

    template <>
    void fromRgba(sChunkData& chunk, const uint32_t* r, const uint32_t* g, const uint32_t* b, const uint32_t* a)
    {
        for (uint32_t y = 0; y < chunk.height; y++)
        {
            uint32_t idx = chunk.width * y;
            auto out = chunk.bitmap.data() + y * chunk.pitch;
            for (uint32_t x = 0; x < chunk.width; x++)
            {
                const auto* ur = reinterpret_cast<const uint8_t*>(&r[idx]);
                const auto* ug = reinterpret_cast<const uint8_t*>(&g[idx]);
                const auto* ub = reinterpret_cast<const uint8_t*>(&b[idx]);
                const auto* ua = reinterpret_cast<const uint8_t*>(&a[idx]);
                out[0] = ur[1];
                out[1] = ug[1];
                out[2] = ub[1];
                out[3] = ua[1];
                out += 4;
                idx++;
            }
        }
    }

    bool isValidFormat(const PSD_HEADER& header)
    {
        const uint16_t version = helpers::read_uint16(reinterpret_cast<const uint8_t*>(&header.version));
        return version == 1
            && header.signature[0] == '8'
            && header.signature[1] == 'B'
            && header.signature[2] == 'P'
            && header.signature[3] == 'S';
    }
} // namespace

bool cFormatPsd::isSupported(cFile& file, Buffer& buffer) const
{
    if (readBuffer(file, buffer, sizeof(PSD_HEADER)) == false)
    {
        return false;
    }

    const auto h = reinterpret_cast<const PSD_HEADER*>(buffer.data());
    return isValidFormat(*h);
}

void cFormatPsd::decodePreview(const Buffer& jpegData, uint32_t fullWidth, uint32_t fullHeight)
{
    if (jpegData.empty())
    {
        return;
    }

    auto bitmap = cJpegDecoder::decodeThumbnail(jpegData.data(), static_cast<uint32_t>(jpegData.size()));
    if (bitmap.width == 0 || bitmap.height == 0)
    {
        return;
    }

    sPreviewData data;
    data.bitmap = std::move(bitmap.data);
    data.width = bitmap.width;
    data.height = bitmap.height;
    data.pitch = bitmap.pitch;
    data.bpp = bitmap.bpp;
    data.format = bitmap.format;
    data.fullImageWidth = fullWidth;
    data.fullImageHeight = fullHeight;

    // cLog::Debug("PSD preview: {}x{}, full: {}x{}", bitmap.width, bitmap.height, fullWidth, fullHeight);
    signalPreviewReady(std::move(data));
}

bool cFormatPsd::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (openFile(file, filename, info) == false)
    {
        return false;
    }

    PSD_HEADER header;
    if (sizeof(PSD_HEADER) != file.read(&header, sizeof(PSD_HEADER)))
    {
        cLog::Error("Can't read PSD header.");
        return false;
    }

    if (isValidFormat(header) == false)
    {
        cLog::Error("Not valid PSD file.");
        return false;
    }

    const auto colorMode = static_cast<ColorMode>(helpers::read_uint16(reinterpret_cast<uint8_t*>(&header.colorMode)));
    if (colorMode != ColorMode::RGB && colorMode != ColorMode::CMYK && colorMode != ColorMode::GRAYSCALE)
    {
        cLog::Error("Unsupported color mode: {}.", modeToString(colorMode));
        return false;
    }

    const uint32_t depth = helpers::read_uint16(reinterpret_cast<uint8_t*>(&header.depth));
    if (depth != 8 && depth != 16 && depth != 32)
    {
        cLog::Error("Unsupported depth: {}.", depth);
        return false;
    }
    const uint32_t bytesPerComponent = depth / 8;

    const uint32_t channels = helpers::read_uint16(reinterpret_cast<uint8_t*>(&header.channels));

    // skip Color Mode Data Block
    if (skipNextBlock(file) == false)
    {
        cLog::Error("Can't read color mode data block.");
        return false;
    }

    // read Image Resources Block (extract ICC profile and thumbnail)
    Buffer iccProfile;
    Buffer thumbnailJpeg;
    if (readImageResources(file, iccProfile, thumbnailJpeg) == false)
    {
        cLog::Error("Can't read image resources block.");
        return false;
    }

    const uint32_t fullWidth = helpers::read_uint32(reinterpret_cast<uint8_t*>(&header.columns));
    const uint32_t fullHeight = helpers::read_uint32(reinterpret_cast<uint8_t*>(&header.rows));
    decodePreview(thumbnailJpeg, fullWidth, fullHeight);

    // skip Layer and Mask Information Block
    if (skipNextBlock(file) == false)
    {
        cLog::Error("Can't read layer and mask information block.");
        return false;
    }

    // Image Data Block
    CompressionMethod compression;
    if (sizeof(uint16_t) != file.read(&compression, sizeof(uint16_t)))
    {
        cLog::Error("Can't read compression info.");
        return false;
    }
    compression = static_cast<CompressionMethod>(helpers::read_uint16(reinterpret_cast<uint8_t*>(&compression)));
    if (compression != CompressionMethod::RAW && compression != CompressionMethod::RLE)
    {
        cLog::Error("Unsupported compression: {}.", static_cast<uint32_t>(compression));
        return false;
    }

    chunk.width = fullWidth;
    chunk.height = fullHeight;

    // this will be needed for RLE decompression
    std::vector<uint16_t> linesLengths;
    if (compression == CompressionMethod::RLE)
    {
        linesLengths.resize(channels * chunk.height);
        for (uint32_t ch = 0; ch < channels; ch++)
        {
            const uint32_t pos = chunk.height * ch;

            if (chunk.height * sizeof(uint16_t) != file.read(&linesLengths[pos], chunk.height * sizeof(uint16_t)))
            {
                cLog::Error("Can't read length of lines");
                return false;
            }
        }

        // convert from big-endian
        for (uint32_t i = 0; i < chunk.height * channels; i++)
        {
            linesLengths[i] = helpers::read_uint16(reinterpret_cast<uint8_t*>(&linesLengths[i]));
        }
    }

    info.bppImage = depth * channels;

    // RLE worst case: each literal byte needs a count byte, so ~2x expansion
    const uint32_t maxLineLength = chunk.width * 2 * bytesPerComponent;
    std::vector<uint8_t> buffer(maxLineLength);

    // create separate buffers for each channel (up to 56 buffers by spec)
    std::vector<std::vector<uint8_t>> chBufs(channels);
    for (uint32_t ch = 0; ch < channels; ch++)
    {
        chBufs[ch].resize(chunk.width * chunk.height * bytesPerComponent);
    }

    // read all channels and extra if available
    for (uint32_t ch = 0; ch < channels && m_stop == false; ch++)
    {
        uint32_t pos = 0;
        for (uint32_t row = 0; row < chunk.height && m_stop == false; row++)
        {
            if (compression == CompressionMethod::RLE)
            {
                // linesLengths stores compressed byte counts per scanline
                uint32_t lineLength = linesLengths[ch * chunk.height + row];
                if (maxLineLength < lineLength)
                {
                    cLog::Warning("Invalid line length: {}.", lineLength);
                    lineLength = maxLineLength;
                }

                const auto bytesRead = file.read(buffer.data(), lineLength);
                if (lineLength != bytesRead)
                {
                    cLog::Warning("Can't read image data block.");
                }

                decodeRle(chBufs[ch].data() + pos, buffer.data(), lineLength);
            }
            else
            {
                uint32_t lineLength = chunk.width * bytesPerComponent;

                const auto bytesRead = file.read(chBufs[ch].data() + pos, lineLength);
                if (lineLength != bytesRead)
                {
                    cLog::Warning("Can't read image data block.");
                }
            }

            updateProgress(static_cast<float>(ch * chunk.height + row) / (channels * chunk.height));

            pos += chunk.width * bytesPerComponent;
        }
    }

    if (m_stop)
    {
        return false;
    }

    if (colorMode == ColorMode::RGB)
    {
        if (channels == 3)
        {
            setupBitmap(chunk, info, 24, ePixelFormat::RGB, "psd");
            interleaveChannels(chunk, chBufs, 3, bytesPerComponent);
        }
        else
        {
            setupBitmap(chunk, info, 32, ePixelFormat::RGBA, "psd");
            switch (depth)
            {
            case 8:
                fromRgba(chunk, chBufs[0].data(), chBufs[1].data(), chBufs[2].data(), chBufs[3].data());
                break;

            case 16:
                fromRgba(chunk,
                         reinterpret_cast<const uint16_t*>(chBufs[0].data()),
                         reinterpret_cast<const uint16_t*>(chBufs[1].data()),
                         reinterpret_cast<const uint16_t*>(chBufs[2].data()),
                         reinterpret_cast<const uint16_t*>(chBufs[3].data()));
                break;

            case 32:
                fromRgba(chunk,
                         reinterpret_cast<const uint32_t*>(chBufs[0].data()),
                         reinterpret_cast<const uint32_t*>(chBufs[1].data()),
                         reinterpret_cast<const uint32_t*>(chBufs[2].data()),
                         reinterpret_cast<const uint32_t*>(chBufs[3].data()));
                break;
            }
        }
    }
    else if (colorMode == ColorMode::CMYK)
    {
        // Upload raw CMYK as RGBA with ePixelFormat::CMYK — GPU shader converts:
        // PSD stores 0=full ink, 255=no ink → shader does rgb = texel.rgb * texel.a
        if (channels >= 4)
        {
            setupBitmap(chunk, info, 32, ePixelFormat::CMYK, "psd");
            interleaveChannels(chunk, chBufs, 4, bytesPerComponent);
        }
    }
    else if (colorMode == ColorMode::GRAYSCALE)
    {
        if (channels == 2)
        {
            // Gray + Alpha → GPU handles gray expansion via GL swizzle
            setupBitmap(chunk, info, 16, ePixelFormat::LuminanceAlpha, "psd");
            interleaveChannels(chunk, chBufs, 2, bytesPerComponent);
        }
        else if (channels == 1)
        {
            // Single gray channel → GPU handles gray expansion via GL swizzle
            setupBitmap(chunk, info, 8, ePixelFormat::Luminance, "psd");
            interleaveChannels(chunk, chBufs, 1, bytesPerComponent);
        }
    }

    if (applyIccProfile(chunk, iccProfile.data(), static_cast<uint32_t>(iccProfile.size())))
    {
        info.formatName = "psd/icc";
    }

    return true;
}

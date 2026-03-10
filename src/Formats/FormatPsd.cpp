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

#if defined(EXIF_SUPPORT)
#include <libexif/exif-data.h>
#endif

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
    constexpr uint16_t PSD_EXIF_DATA_RESOURCE = 0x0422;

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

    // Read Image Resources Block and extract ICC profile, thumbnail, and EXIF data.
    // Returns false on read error.
    bool readImageResources(cFile& file, Buffer& iccProfile, Buffer& thumbnailJpeg, Buffer& exifData)
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
            else if (resourceId == PSD_EXIF_DATA_RESOURCE && dataSize > 0)
            {
                exifData.resize(dataSize);
                if (dataSize != file.read(exifData.data(), dataSize))
                {
                    exifData.clear();
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
        uint32_t bytesRead = 0;
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

    bool isValidFormat(const PSD_HEADER& header)
    {
        const uint16_t version = helpers::read_uint16(reinterpret_cast<const uint8_t*>(&header.version));
        return version == 1
            && header.signature[0] == '8'
            && header.signature[1] == 'B'
            && header.signature[2] == 'P'
            && header.signature[3] == 'S';
    }

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

    info.formatName = "psd";

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

    // read Image Resources Block (extract ICC profile, thumbnail, and EXIF)
    Buffer iccProfile;
    Buffer thumbnailJpeg;
    Buffer exifData;
    if (readImageResources(file, iccProfile, thumbnailJpeg, exifData) == false)
    {
        cLog::Error("Can't read image resources block.");
        return false;
    }

    const uint32_t fullWidth = helpers::read_uint32(reinterpret_cast<uint8_t*>(&header.columns));
    const uint32_t fullHeight = helpers::read_uint32(reinterpret_cast<uint8_t*>(&header.rows));
    decodePreview(thumbnailJpeg, fullWidth, fullHeight);

#if defined(EXIF_SUPPORT)
    if (exifData.empty() == false)
    {
        auto* ed = exif_data_new_from_data(exifData.data(), static_cast<unsigned>(exifData.size()));
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
#endif

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

    // Determine output format early so setupBitmap/signalBitmapAllocated fires
    // before the heavy channel reading loop
    uint32_t outBpp = 0;
    uint32_t outChannels = 0;
    auto outFormat = ePixelFormat::RGB;

    if (colorMode == ColorMode::RGB)
    {
        if (channels == 3)
        {
            outBpp = 24;
            outChannels = 3;
            outFormat = ePixelFormat::RGB;
        }
        else
        {
            outBpp = 32;
            outChannels = 4;
            outFormat = ePixelFormat::RGBA;
        }
    }
    else if (colorMode == ColorMode::CMYK)
    {
        if (channels >= 4)
        {
            outBpp = 32;
            outChannels = 4;
            outFormat = ePixelFormat::CMYK;
        }
    }
    else if (colorMode == ColorMode::GRAYSCALE)
    {
        if (channels == 2)
        {
            outBpp = 16;
            outChannels = 2;
            outFormat = ePixelFormat::LuminanceAlpha;
        }
        else if (channels == 1)
        {
            outBpp = 8;
            outChannels = 1;
            outFormat = ePixelFormat::Luminance;
        }
    }

    if (outBpp == 0)
    {
        cLog::Error("Unsupported channel configuration: {} mode, {} channels.", modeToString(colorMode), channels);
        return false;
    }

    // Allocate bitmap and apply ICC early so infobar shows format/dimensions
    // from the start. The row-by-row decode loop fills the bitmap progressively.
    chunk.allocate(chunk.width, chunk.height, outBpp, outFormat);

    if (applyIccProfile(chunk, iccProfile.data(), static_cast<uint32_t>(iccProfile.size())))
    {
        info.formatName = "psd/icc";
    }

    signalImageInfo();

    // Compute file offsets for each (channel, row) so we can read row-interleaved:
    // for each row, seek to each channel's data, decode, interleave, update readyHeight.
    const auto dataStart = file.getOffset();
    std::vector<long> channelRowOffsets(channels * chunk.height);

    if (compression == CompressionMethod::RLE)
    {
        long offset = dataStart;
        for (uint32_t ch = 0; ch < channels; ch++)
        {
            for (uint32_t row = 0; row < chunk.height; row++)
            {
                channelRowOffsets[ch * chunk.height + row] = offset;
                offset += linesLengths[ch * chunk.height + row];
            }
        }
    }
    else
    {
        const long rowBytes = chunk.width * bytesPerComponent;
        for (uint32_t ch = 0; ch < channels; ch++)
        {
            for (uint32_t row = 0; row < chunk.height; row++)
            {
                channelRowOffsets[ch * chunk.height + row] = dataStart + static_cast<long>(ch * chunk.height + row) * rowBytes;
            }
        }
    }

    // Read rows in batches: one seek per channel per batch, then sequential reads.
    // Reduces seek count from (height × channels) to (height/batch × channels)
    // while keeping memory small (~2.7MB for 20978px × 4ch × 32 rows).
    constexpr uint32_t BatchSize = 16;
    const uint32_t rowBytes = chunk.width * bytesPerComponent;

    std::vector<std::vector<uint8_t>> batchBufs(channels);
    for (uint32_t ch = 0; ch < channels; ch++)
    {
        batchBufs[ch].resize(BatchSize * rowBytes);
    }

    // RLE worst case: each literal byte needs a count byte, so ~2x expansion
    const uint32_t maxLineLength = chunk.width * 2 * bytesPerComponent;
    std::vector<uint8_t> rleBuffer(compression == CompressionMethod::RLE ? maxLineLength : 0);

    signalBitmapAllocated();

    for (uint32_t batchStart = 0; batchStart < chunk.height && m_stop == false; batchStart += BatchSize)
    {
        const uint32_t batchEnd = std::min(batchStart + BatchSize, chunk.height);

        // Read all batch rows for each channel (sequential within channel)
        for (uint32_t ch = 0; ch < channels && m_stop == false; ch++)
        {
            file.seek(channelRowOffsets[ch * chunk.height + batchStart], SEEK_SET);

            for (uint32_t row = batchStart; row < batchEnd && m_stop == false; row++)
            {
                auto dst = batchBufs[ch].data() + (row - batchStart) * rowBytes;

                if (compression == CompressionMethod::RLE)
                {
                    uint32_t lineLength = linesLengths[ch * chunk.height + row];
                    if (maxLineLength < lineLength)
                    {
                        cLog::Warning("Invalid line length: {}.", lineLength);
                        lineLength = maxLineLength;
                    }

                    const auto bytesRead = file.read(rleBuffer.data(), lineLength);
                    if (lineLength != bytesRead)
                    {
                        cLog::Warning("Can't read image data block.");
                    }

                    decodeRle(dst, rleBuffer.data(), lineLength);
                }
                else
                {
                    const auto bytesRead = file.read(dst, rowBytes);
                    if (rowBytes != bytesRead)
                    {
                        cLog::Warning("Can't read image data block.");
                    }
                }
            }
        }

        // Interleave batch rows into bitmap (take MSB for >8-bit depth)
        for (uint32_t row = batchStart; row < batchEnd; row++)
        {
            auto out = chunk.bitmap.data() + row * chunk.pitch;
            const auto rowOff = (row - batchStart) * rowBytes;
            for (uint32_t x = 0; x < chunk.width; x++)
            {
                const uint32_t idx = rowOff + x * bytesPerComponent;
                for (uint32_t ch = 0; ch < outChannels; ch++)
                {
                    out[ch] = batchBufs[ch][idx];
                }
                out += outChannels;
            }
        }

        chunk.readyHeight.store(batchEnd, std::memory_order_release);
        updateProgress(static_cast<float>(batchEnd) / chunk.height);
    }

    if (m_stop)
    {
        return false;
    }

    return true;
}

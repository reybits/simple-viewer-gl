/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatPng.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/Helpers.h"
#include "Common/ImageInfo.h"
#include "Libs/PngReader.h"
#include "Log/Log.h"

#include <cstring>
#include <zlib.h>

namespace
{
    uint32_t ReadU32BE(const uint8_t* p)
    {
        return (static_cast<uint32_t>(p[0]) << 24)
            | (static_cast<uint32_t>(p[1]) << 16)
            | (static_cast<uint32_t>(p[2]) << 8)
            | static_cast<uint32_t>(p[3]);
    }

    constexpr uint32_t ChunkType(const char (&s)[5])
    {
        return (static_cast<uint32_t>(static_cast<uint8_t>(s[0])) << 24)
            | (static_cast<uint32_t>(static_cast<uint8_t>(s[1])) << 16)
            | (static_cast<uint32_t>(static_cast<uint8_t>(s[2])) << 8)
            | static_cast<uint32_t>(static_cast<uint8_t>(s[3]));
    }

    // Paeth predictor (PNG filter type 4)
    uint8_t PaethPredictor(uint8_t a, uint8_t b, uint8_t c)
    {
        auto p = static_cast<int>(a) + b - c;
        auto pa = std::abs(p - a);
        auto pb = std::abs(p - b);
        auto pc = std::abs(p - c);
        if (pa <= pb && pa <= pc)
        {
            return a;
        }
        if (pb <= pc)
        {
            return b;
        }
        return c;
    }

} // namespace

bool cFormatPng::isSupported(cFile& file, Buffer& buffer) const
{
    if (readBuffer(file, buffer, cPngReader::HeaderSize) == false)
    {
        return false;
    }

    return cPngReader::isValid(buffer.data(), file.getSize());
}

bool cFormatPng::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (openFile(file, filename, info) == false)
    {
        return false;
    }

    // Detect Apple CgBI PNG: check if first chunk after 8-byte signature is "CgBI"
    uint8_t header[20];
    if (file.read(header, 20) == 20)
    {
        uint32_t firstChunkType = ReadU32BE(header + 12);
        if (firstChunkType == ChunkType("CgBI"))
        {
            file.seek(0, SEEK_SET);
            return loadCgBI(file, chunk, info);
        }
    }
    file.seek(0, SEEK_SET);

    cPngReader reader;
    reader.setProgressCallback([this](float progress) {
        updateProgress(progress);
    });
    reader.setBitmapAllocatedCallback([this, &reader, &info]() {
        info.formatName = reader.getIccProfile().empty()
            ? "png"
            : "png/icc";
        signalBitmapAllocated();
    });
    reader.setStopFlag(&m_stop);

    // ICC LUT generated inside loadPng() — applied on GPU during rendering
    return reader.loadPng(chunk, info, file);
}

bool cFormatPng::loadCgBI(cFile& file, sChunkData& chunk, sImageInfo& info)
{
    // Read entire file into memory for chunk parsing
    const auto fileSize = file.getSize();
    std::vector<uint8_t> fileData(fileSize);
    file.seek(0, SEEK_SET);
    if (file.read(fileData.data(), fileSize) != static_cast<uint32_t>(fileSize))
    {
        cLog::Error("CgBI: failed to read file.");
        return false;
    }

    // Skip 8-byte PNG signature
    uint32_t offset = 8;
    const auto* data = fileData.data();

    uint32_t width = 0;
    uint32_t height = 0;
    uint8_t bitDepth = 0;
    uint8_t colorType = 0;

    // Collect compressed IDAT data
    std::vector<uint8_t> compressedData;

    while (offset + 12 <= static_cast<uint32_t>(fileSize))
    {
        uint32_t chunkLen = ReadU32BE(data + offset);
        uint32_t chunkType = ReadU32BE(data + offset + 4);
        const uint8_t* chunkData = data + offset + 8;

        if (offset + 12 + chunkLen > static_cast<uint32_t>(fileSize))
        {
            break;
        }

        if (chunkType == ChunkType("IHDR"))
        {
            if (chunkLen < 13)
            {
                cLog::Error("CgBI: invalid IHDR chunk.");
                return false;
            }
            width = ReadU32BE(chunkData);
            height = ReadU32BE(chunkData + 4);
            bitDepth = chunkData[8];
            colorType = chunkData[9];
        }
        else if (chunkType == ChunkType("IDAT"))
        {
            compressedData.insert(compressedData.end(), chunkData, chunkData + chunkLen);
        }
        else if (chunkType == ChunkType("IEND"))
        {
            break;
        }

        // length + type(4) + data + CRC(4)
        offset += 12 + chunkLen;
    }

    if (width == 0 || height == 0)
    {
        cLog::Error("CgBI: missing IHDR.");
        return false;
    }

    if (bitDepth != 8)
    {
        cLog::Error("CgBI: unsupported bit depth {}.", bitDepth);
        return false;
    }

    // Determine pixel layout
    uint32_t channels = 0;
    if (colorType == 2) // RGB (stored as BGR in CgBI)
    {
        channels = 3;
        chunk.format = ePixelFormat::BGR;
    }
    else if (colorType == 6) // RGBA (stored as BGRA in CgBI)
    {
        channels = 4;
        chunk.format = ePixelFormat::BGRA;
    }
    else
    {
        cLog::Error("CgBI: unsupported color type {}.", colorType);
        return false;
    }

    info.bppImage = bitDepth * channels;
    chunk.bpp = channels * 8;
    chunk.width = width;
    chunk.height = height;
    chunk.pitch = helpers::calculatePitch(width, chunk.bpp);

    // Raw scanline includes 1 filter byte per row
    const auto rawSize = static_cast<size_t>(height) * (static_cast<size_t>(width) * channels + 1);

    // CgBI uses raw deflate (no zlib header) — inflate with negative windowBits
    std::vector<uint8_t> rawData(rawSize);

    z_stream zs = {};
    if (inflateInit2(&zs, -15) != Z_OK)
    {
        cLog::Error("CgBI: inflateInit2 failed.");
        return false;
    }

    zs.next_in = compressedData.data();
    zs.avail_in = static_cast<uInt>(compressedData.size());
    zs.next_out = rawData.data();
    zs.avail_out = static_cast<uInt>(rawData.size());

    int ret = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);

    if (ret != Z_STREAM_END)
    {
        cLog::Error("CgBI: inflate failed ({}).", ret);
        return false;
    }

    // Allocate output bitmap
    chunk.resizeBitmap(chunk.pitch, height);

    info.formatName = "png/cgbi";
    signalBitmapAllocated();

    // Reconstruct PNG-filtered scanlines (BGRA/BGR channel order handled by GPU)
    const uint32_t srcStride = width * channels + 1; // +1 for filter byte
    const uint8_t* prevRow = nullptr;

    for (uint32_t y = 0; y < height; y++)
    {
        if (m_stop)
        {
            return false;
        }

        const auto* src = rawData.data() + static_cast<size_t>(y) * srcStride;
        auto* dst = chunk.bitmap.data() + static_cast<size_t>(y) * chunk.pitch;
        auto filterType = src[0];
        const auto* filtered = src + 1;

        // Apply PNG filter to reconstruct raw pixel data
        for (uint32_t i = 0; i < width * channels; i++)
        {
            auto a = (i >= channels) ? dst[i - channels] : uint8_t(0);
            auto b = (prevRow != nullptr) ? prevRow[i] : uint8_t(0);
            auto c = (prevRow != nullptr && i >= channels) ? prevRow[i - channels] : uint8_t(0);

            switch (filterType)
            {
            case 0: // None
                dst[i] = filtered[i];
                break;
            case 1: // Sub
                dst[i] = filtered[i] + a;
                break;
            case 2: // Up
                dst[i] = filtered[i] + b;
                break;
            case 3: // Average
                dst[i] = filtered[i] + static_cast<uint8_t>((static_cast<uint32_t>(a) + b) / 2);
                break;
            case 4: // Paeth
                dst[i] = filtered[i] + PaethPredictor(a, b, c);
                break;
            }
        }

        prevRow = dst;
        chunk.readyHeight.store(y + 1, std::memory_order_release);
        updateProgress(static_cast<float>(y + 1) / height);
    }

    return true;
}

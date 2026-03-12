/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "PngReader.h"
#include "Common/ChunkData.h"
#include "Common/Cms.h"
#include "Common/File.h"
#include "Common/Helpers.h"
#include "Common/ImageInfo.h"
#include "Log/Log.h"

#include <cstring>
#include <png.h>
#include <thread>

class cPngWrapper final
{
public:
    ~cPngWrapper()
    {
        destroy();
    }

    bool createMemoryReader(const uint8_t* data, uint32_t size)
    {
        if (create())
        {
            m_data   = data;
            m_remain = size;
            m_offset = 0u;

            png_set_read_fn(m_png, this, MemoryReader);

            return true;
        }

        return false;
    }

    bool createFileReader(cFile& file)
    {
        if (create())
        {
            png_init_io(m_png, static_cast<FILE*>(file.getHandle()));
            png_set_sig_bytes(m_png, cPngReader::HeaderSize);

            return true;
        }

        return false;
    }

    png_structp getPng() const
    {
        return m_png;
    }

    png_infop getInfo() const
    {
        return m_info;
    }

private:
    bool create()
    {
        destroy();

        m_png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (m_png == nullptr)
        {
            cLog::Error("png_create_read_struct failed.");
            return false;
        }

        m_info = png_create_info_struct(m_png);
        if (m_info == nullptr)
        {
            cLog::Error("png_create_info_struct failed.");
            return false;
        }

        return true;
    }

    void destroy()
    {
        m_data   = nullptr;
        m_remain = 0u;
        m_offset = 0u;

        if (m_png != nullptr)
        {
            png_destroy_read_struct(&m_png, &m_info, nullptr);
            m_png  = nullptr;
            m_info = nullptr;
        }
    }

    uint32_t read(uint8_t* out, uint32_t size)
    {
        size = size <= m_remain
            ? size
            : m_remain;

        ::memcpy(out, &m_data[m_offset], size);
        m_offset += size;
        m_remain -= size;

        return size;
    }

    static void MemoryReader(png_structp png, png_bytep outBytes, png_size_t byteCountToRead)
    {
        auto reader = static_cast<cPngWrapper*>(png_get_io_ptr(png));
        if (reader != nullptr)
        {
            reader->read(outBytes, byteCountToRead);
        }
    }

private:
    png_structp m_png = nullptr;
    png_infop m_info  = nullptr;

    const uint8_t* m_data = nullptr;
    uint32_t m_remain     = 0u;
    uint32_t m_offset     = 0u;
};

namespace
{
    void* locateICCProfile(const png_structp png, const png_infop info, uint32_t& iccProfileSize)
    {
        png_charp name;
        int comp_type;
#if ((PNG_LIBPNG_VER_MAJOR << 8) | PNG_LIBPNG_VER_MINOR << 0) < ((1 << 8) | (5 << 0))
        png_charp icc;
#else // >= libpng 1.5.0
        png_bytep icc;
#endif
        png_uint_32 size;
        if (png_get_iCCP(png, info, &name, &comp_type, &icc, &size) == PNG_INFO_iCCP)
        {
            cLog::Debug("-- ICC profile");
            cLog::Debug("  Name             : {}", name);
            cLog::Debug("  Compression type : {}", comp_type);
            cLog::Debug("  Size             : {}", size);

            iccProfileSize = size;
            return icc;
        }

        return nullptr;
    }

} // namespace

cPngReader::cPngReader()
{
}

cPngReader::~cPngReader()
{
}

bool cPngReader::isValid(const uint8_t* data, uint32_t size)
{
    if (size >= HeaderSize)
    {
        uint8_t header[HeaderSize];
        ::memcpy(header, data, sizeof(header));
        return png_sig_cmp(header, 0, sizeof(header)) == 0;
    }

    return false;
}

bool cPngReader::loadPng(sChunkData& chunk, sImageInfo& info, const uint8_t* data, uint32_t size)
{
    if (isValid(data, size) == false)
    {
        cLog::Error("Unrecognized PNG frame format.");
        return false;
    }

    cPngWrapper wrapper;
    if (wrapper.createMemoryReader(data, size) == false)
    {
        cLog::Error("Can't create PNG memory reader.");
        return false;
    }

    return doLoadPNG(wrapper, chunk, info);
}

bool cPngReader::loadPng(sChunkData& chunk, sImageInfo& info, cFile& file)
{
    info.fileSize = file.getSize();

    uint8_t header[HeaderSize];
    if (file.read(&header, HeaderSize) != HeaderSize
        || isValid(header, file.getSize()) == false)
    {
        cLog::Error("Unrecognized PNG file format.");
        return false;
    }

    cPngWrapper wrapper;
    if (wrapper.createFileReader(file) == false)
    {
        cLog::Error("Can't create PNG file reader.");
        return false;
    }

    return doLoadPNG(wrapper, chunk, info);
}

bool cPngReader::doLoadPNG(const cPngWrapper& wrapper, sChunkData& chunk, sImageInfo& imgInfo)
{
    auto png  = wrapper.getPng();
    auto info = wrapper.getInfo();

    if (setjmp(png_jmpbuf(png)) != 0)
    {
        cLog::Error("Can't read PNG data.");
        return false;
    }

    png_read_info(png, info);

    // Store original bpp image
    imgInfo.bppImage = png_get_bit_depth(png, info) * png_get_channels(png, info);

    auto colorType = png_get_color_type(png, info);

    // Setup transformations.

    cLog::Debug("Source color type: {}.", colorType);
    if (colorType == PNG_COLOR_TYPE_PALETTE)
    {
        cLog::Debug("Converting palette to RGB.");
        png_set_palette_to_rgb(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS))
    {
        cLog::Debug("Converting tRNS to alpha.");
        png_set_tRNS_to_alpha(png);
    }
    if (png_get_bit_depth(png, info) == 16)
    {
        cLog::Debug("Stripping 16-bit depth to 8-bit.");
        png_set_strip_16(png);
    }

    // Update info structure to apply transformations
    png_read_update_info(png, info);

    chunk.width  = png_get_image_width(png, info);
    chunk.height = png_get_image_height(png, info);
    chunk.bpp    = png_get_bit_depth(png, info) * png_get_channels(png, info);
    chunk.pitch  = helpers::calculatePitch(chunk.width, chunk.bpp);

    auto srcPitch = png_get_rowbytes(png, info);
    if (chunk.pitch < srcPitch)
    {
        cLog::Error("Source pitch {} is larger than destination pitch {}.", srcPitch, chunk.pitch);
    }

    colorType = png_get_color_type(png, info);
    cLog::Debug("Result color type: {}, bpp: {}.", colorType, chunk.bpp);

    // #define PNG_COLOR_MASK_PALETTE    1
    // #define PNG_COLOR_MASK_COLOR      2
    // #define PNG_COLOR_MASK_ALPHA      4
    if (colorType == PNG_COLOR_TYPE_GRAY) // 0b00000000 (gray)
    {
        chunk.format = ePixelFormat::Alpha;
    }
    else if (colorType == PNG_COLOR_MASK_ALPHA) // 0b00000100 (gray + alpha)
    {
        chunk.format = ePixelFormat::LuminanceAlpha;
    }
    else if (colorType == PNG_COLOR_TYPE_RGB) // 0b00000010 (rgb)
    {
        chunk.format = ePixelFormat::RGB;
    }
    else if (colorType == PNG_COLOR_TYPE_RGB_ALPHA) // 0b00000110 (rgb + alpha)
    {
        chunk.format = ePixelFormat::RGBA;
    }
    else
    {
        cLog::Error("Unexpected PNG color type.");
    }

    // Extract ICC profile early (available after png_read_info)
    uint32_t iccProfileSize = 0;
    auto iccProfileData     = locateICCProfile(png, info, iccProfileSize);
    m_iccProfile.resize(iccProfileSize);
    if (iccProfileData != nullptr && iccProfileSize != 0)
    {
        ::memcpy(m_iccProfile.data(), iccProfileData, iccProfileSize);
    }

    // Allocate band buffer
    constexpr uint32_t BandRows = 8192;
    chunk.bandHeight            = BandRows < chunk.height
        ? BandRows
        : chunk.height;
    chunk.resizeBitmap(chunk.pitch, chunk.bandHeight);

    // Generate 3D LUT from ICC profile (applied on GPU during rendering)
    if (m_iccProfile.empty() == false)
    {
        chunk.lutData = cms::generateLut3D(
            m_iccProfile.data(), static_cast<uint32_t>(m_iccProfile.size()), chunk.format);
        if (chunk.lutData.empty() == false)
        {
            chunk.effects |= eEffect::Lut;
        }
    }

    if (m_allocated != nullptr)
    {
        m_allocated();
    }

    for (uint32_t y = 0; y < chunk.height; y++)
    {
        if (m_stop != nullptr && *m_stop)
        {
            return false;
        }

        // Wait for ring buffer room
        while (m_stop != nullptr && *m_stop == false)
        {
            auto consumed = chunk.consumedHeight.load(std::memory_order_acquire);
            if (y - consumed < chunk.bandHeight)
            {
                break;
            }
            std::this_thread::yield();
        }

        auto row = chunk.rowPtr(y);
        png_read_row(png, row, nullptr);

        chunk.readyHeight.store(y + 1, std::memory_order_release);
        updateProgress(static_cast<float>(y + 1) / chunk.height);
    }

    return true;
}

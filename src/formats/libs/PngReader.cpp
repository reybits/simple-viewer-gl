/**********************************************\
*
*  Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "PngReader.h"
#include "common/bitmap_description.h"
#include "common/file.h"
#include "common/helpers.h"
#include "log/Log.h"

#include <cstring>
#include <png.h>

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
            m_data = data;
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
        m_data = nullptr;
        m_remain = 0u;
        m_offset = 0u;

        if (m_png != nullptr)
        {
            png_destroy_read_struct(&m_png, &m_info, nullptr);
            m_png = nullptr;
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
    png_infop m_info = nullptr;

    const uint8_t* m_data = nullptr;
    uint32_t m_remain = 0u;
    uint32_t m_offset = 0u;
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
            // cLog::Debug("name: {}", name);
            // cLog::Debug("comp_type: {}", comp_type);
            // cLog::Debug("size: {}", size);

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

bool cPngReader::loadPng(sBitmapDescription& desc, const uint8_t* data, uint32_t size)
{
    if (isValid(data, size) == false)
    {
        cLog::Error("Frame is not recognized as a PNG format.");
        return false;
    }

    cPngWrapper wrapper;
    if (wrapper.createMemoryReader(data, size) == false)
    {
        cLog::Error("Cannot create PNG memory reader.");
        return false;
    }

    return doLoadPNG(wrapper, desc);
}

bool cPngReader::loadPng(sBitmapDescription& desc, cFile& file)
{
    desc.size = file.getSize();

    uint8_t header[HeaderSize];
    if (file.read(&header, HeaderSize) != HeaderSize
        && isValid(header, file.getSize()) == false)
    {
        cLog::Error("Is not recognized as a PNG file.");
        return false;
    }

    cPngWrapper wrapper;
    if (wrapper.createFileReader(file) == false)
    {
        cLog::Error("Cannot create PNG file reader.");
        return false;
    }

    return doLoadPNG(wrapper, desc);
}

bool cPngReader::doLoadPNG(const cPngWrapper& wrapper, sBitmapDescription& desc)
{
    auto png = wrapper.getPng();
    auto info = wrapper.getInfo();

    if (setjmp(png_jmpbuf(png)) != 0)
    {
        cLog::Error("Error during PNG read.");
        return false;
    }

    png_read_info(png, info);

    // Store original bpp image
    desc.bppImage = png_get_bit_depth(png, info) * png_get_channels(png, info);

    auto colorType = png_get_color_type(png, info);

    // Setup transformations.

    // cLog::Debug("src color type: {}.", colorType);
    if (colorType == PNG_COLOR_TYPE_PALETTE)
    {
        // cLog::Debug("palette -> rgb.");
        png_set_palette_to_rgb(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS))
    {
        // cLog::Debug("tRNS -> alpha.");
        png_set_tRNS_to_alpha(png);
    }
    if (png_get_bit_depth(png, info) == 16)
    {
        // cLog::Debug("16 bit depth -> 8.");
        png_set_strip_16(png);
    }

    // Update info structure to apply transformations
    png_read_update_info(png, info);

    desc.width = png_get_image_width(png, info);
    desc.height = png_get_image_height(png, info);
    desc.bpp = png_get_bit_depth(png, info) * png_get_channels(png, info);
    desc.pitch = helpers::calculatePitch(desc.width, desc.bpp);

    auto srcPitch = png_get_rowbytes(png, info);
    if (desc.pitch < srcPitch)
    {
        cLog::Error("Source pitch {} is larger than destination pitch {}.", srcPitch, desc.pitch);
    }

    colorType = png_get_color_type(png, info);
    // cLog::Debug("res color type: {}.", colorType);
    // cLog::Debug("res bpp: {}.", desc.bpp);

    // #define PNG_COLOR_MASK_PALETTE    1
    // #define PNG_COLOR_MASK_COLOR      2
    // #define PNG_COLOR_MASK_ALPHA      4
    if (colorType == PNG_COLOR_TYPE_GRAY) // 0b00000000 (gray)
    {
        desc.format = GL_ALPHA;
    }
    else if (colorType == PNG_COLOR_MASK_ALPHA) // 0b00000100 (gray + alpha)
    {
        desc.format = GL_LUMINANCE_ALPHA;
    }
    else if (colorType == PNG_COLOR_TYPE_RGB) // 0b00000010 (rgb)
    {
        desc.format = GL_RGB;
    }
    else if (colorType == PNG_COLOR_TYPE_RGB_ALPHA) // 0b00000110 (rgb + alpha)
    {
        desc.format = GL_RGBA;
    }
    else
    {
        cLog::Error("Should't be happened.");
    }

    desc.bitmap.resize(desc.pitch * desc.height);

    // cLog::Debug("png pitch: {}.", rowbytes);
    // cLog::Debug("bitmap pitch: {}.", desc.pitch);

    for (uint32_t y = 0; y < desc.height; y++)
    {
        auto row = desc.bitmap.data() + desc.pitch * y;
        png_read_row(png, row, nullptr);
    }

    uint32_t iccProfileSize = 0;
    auto iccProfile = locateICCProfile(png, info, iccProfileSize);
    m_iccProfile.resize(iccProfileSize);
    if (iccProfile != nullptr && iccProfileSize != 0)
    {
        ::memcpy(m_iccProfile.data(), iccProfile, iccProfileSize);
    }
    // cLog::Debug("\n");

    return true;
}

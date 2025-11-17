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

#include <cstring>
#include <png.h>

namespace
{
    class cPngReadStruct final
    {
    public:
        ~cPngReadStruct()
        {
            destroy();
        }

        bool create()
        {
            destroy();

            m_png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
            if (m_png == nullptr)
            {
                ::printf("(EE) png_create_read_struct failed.\n");
                return false;
            }

            m_info = png_create_info_struct(m_png);
            if (m_info == nullptr)
            {
                ::printf("(EE) png_create_info_struct failed.\n");
                return false;
            }

            return true;
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
        void destroy()
        {
            if (m_png != nullptr)
            {
                png_destroy_read_struct(&m_png, &m_info, nullptr);
                m_png = nullptr;
                m_info = nullptr;
            }
        }

    private:
        png_structp m_png = nullptr;
        png_infop m_info = nullptr;
    };

    class cPngMemoryReader
    {
    public:
        cPngMemoryReader(const uint8_t* data, uint32_t size)
            : m_data(data)
            , m_remain(size)
            , m_offset(0)
        {
        }

        uint32_t read(uint8_t* out, uint32_t size)
        {
            size = size <= m_remain ? size : m_remain;

            ::memcpy(out, &m_data[m_offset], size);
            m_offset += size;
            m_remain -= size;

            return size;
        }

        static void memoryReader(png_structp png, png_bytep outBytes, png_size_t byteCountToRead)
        {
            auto reader = static_cast<cPngMemoryReader*>(png_get_io_ptr(png));
            if (reader != nullptr)
            {
                reader->read(outBytes, byteCountToRead);
            }
        }

    private:
        const uint8_t* m_data;
        uint32_t m_remain;
        uint32_t m_offset;
    };

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
            // ::printf("-- name: %s\n", name);
            // ::printf("-- comp_type: %d\n", comp_type);
            // ::printf("-- size: %u\n", size);

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
        ::printf("(EE) Frame is not recognized as a PNG format.\n");
        return false;
    }

    // initialize stuff
    cPngReadStruct pngStruct;
    if (pngStruct.create() == false)
    {
        ::printf("(EE) png_create_info_struct failed.\n");
        return false;
    }

    auto png = pngStruct.getPng();
    auto info = pngStruct.getInfo();

    cPngMemoryReader pngReader{ data, size };
    png_set_read_fn(png, &pngReader, cPngMemoryReader::memoryReader);

    png_read_info(png, info);

    auto colorType = png_get_color_type(png, info);
    if (colorType == PNG_COLOR_TYPE_PALETTE)
    {
        png_set_palette_to_rgb(png);
    }

#if defined(PNG_1_0_X) || defined(PNG_1_2_X)
    if (colorType == PNG_COLOR_TYPE_GRAY && info->bit_depth < 8)
    {
        // depreceted in libPNG-1.4.2
        png_set_gray_1_2_4_to_8(png);
    }
#endif

    if (png_get_valid(png, info, PNG_INFO_tRNS))
    {
        png_set_tRNS_to_alpha(png);
    }
    if (png_get_bit_depth(png, info) == 16)
    {
        png_set_strip_16(png);
    }
    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
    {
        png_set_gray_to_rgb(png);
    }

    // int number_of_passes = png_set_interlace_handling(png);
    png_read_update_info(png, info);

    desc.width = png_get_image_width(png, info);
    desc.height = png_get_image_height(png, info);
    desc.bpp = png_get_bit_depth(png, info) * png_get_channels(png, info);
    desc.pitch = helpers::calculatePitch(desc.width, desc.bpp); // png_get_rowbytes(png, info);
    if (desc.pitch < png_get_rowbytes(png, info))
    {
        ::printf("(EE) Invalid pitch: %u instead %u.\n", desc.pitch, (uint32_t)png_get_rowbytes(png, info));
    }

    colorType = png_get_color_type(png, info);

    if (colorType != PNG_COLOR_TYPE_RGB_ALPHA && colorType != PNG_COLOR_TYPE_RGB)
    {
        ::printf("(EE) Should't be happened.\n");
    }

    desc.format = colorType == PNG_COLOR_TYPE_RGB_ALPHA ? GL_RGBA : GL_RGB;

    desc.bitmap.resize(desc.pitch * desc.height);
    auto out = desc.bitmap.data();
    std::vector<png_bytep> scanlines(desc.height);
    for (uint32_t y = 0; y < desc.height; y++)
    {
        scanlines[y] = out + desc.pitch * y;
    }
    png_read_image(png, scanlines.data());

    uint32_t iccProfileSize = 0;
    auto iccProfile = locateICCProfile(png, info, iccProfileSize);
    m_iccProfile.resize(iccProfileSize);
    if (iccProfile != nullptr && iccProfileSize != 0)
    {
        ::memcpy(m_iccProfile.data(), iccProfile, iccProfileSize);
    }

    return true;
}

bool cPngReader::loadPng(sBitmapDescription& desc, cFile& file)
{
    desc.size = file.getSize();

    uint8_t header[HeaderSize];
    if (file.read(&header, HeaderSize) != HeaderSize
        && isValid(header, file.getSize()) == false)
    {
        ::printf("(EE) Is not recognized as a PNG file.\n");
        return false;
    }

    // initialize stuff
    cPngReadStruct pngStruct;
    if (pngStruct.create() == false)
    {
        ::printf("(EE) png_create_info_struct failed.\n");
        return false;
    }

    auto png = pngStruct.getPng();
    auto info = pngStruct.getInfo();

    if (setjmp(png_jmpbuf(png)) != 0)
    {
        ::printf("(EE) Error during init_io.\n");
        return false;
    }

    png_init_io(png, (FILE*)file.getHandle());
    png_set_sig_bytes(png, HeaderSize);

    png_read_info(png, info);

    // get real bits per pixel
    desc.bppImage = png_get_bit_depth(png, info) * png_get_channels(png, info);

    auto colorType = png_get_color_type(png, info);
    if (colorType == PNG_COLOR_TYPE_PALETTE)
    {
        png_set_palette_to_rgb(png);
    }

    if (png_get_valid(png, info, PNG_INFO_tRNS))
    {
        png_set_tRNS_to_alpha(png);
    }
    if (png_get_bit_depth(png, info) == 16)
    {
        png_set_strip_16(png);
    }
    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA)
    {
        png_set_gray_to_rgb(png);
    }

    //  int number_of_passes    = png_set_interlace_handling(png);
    png_read_update_info(png, info);

    desc.width = png_get_image_width(png, info);
    desc.height = png_get_image_height(png, info);
    desc.bpp = png_get_bit_depth(png, info) * png_get_channels(png, info);
    desc.pitch = helpers::calculatePitch(desc.width, desc.bpp); // png_get_rowbytes(png, info);
    if (desc.pitch < png_get_rowbytes(png, info))
    {
        ::printf("(EE) Invalid pitch: %u instead %u.\n", desc.pitch, (uint32_t)png_get_rowbytes(png, info));
    }

    colorType = png_get_color_type(png, info);

    if (colorType != PNG_COLOR_TYPE_RGB_ALPHA && colorType != PNG_COLOR_TYPE_RGB)
    {
        ::printf("(EE) Should't be happened.\n");
    }

    desc.format = colorType == PNG_COLOR_TYPE_RGB_ALPHA ? GL_RGBA : GL_RGB;

    // read file
    if (setjmp(png_jmpbuf(png)) != 0)
    {
        ::printf("(EE) Error during read_image.\n");
        return false;
    }

    desc.bitmap.resize(desc.pitch * desc.height);
    auto out = desc.bitmap.data();
    std::vector<png_bytep> scanlines(desc.height);
    for (uint32_t y = 0; y < desc.height; y++)
    {
        scanlines[y] = out + desc.pitch * y;
    }
    png_read_image(png, scanlines.data());

    uint32_t iccProfileSize = 0;
    auto iccProfile = locateICCProfile(png, info, iccProfileSize);
    m_iccProfile.resize(iccProfileSize);
    if (iccProfile != nullptr && iccProfileSize != 0)
    {
        ::memcpy(m_iccProfile.data(), iccProfile, iccProfileSize);
    }

    return true;
}

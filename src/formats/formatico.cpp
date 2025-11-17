/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "formatico.h"
#include "common/bitmap_description.h"
#include "common/file.h"
#include "libs/PngReader.h"

#include <cstring>

#pragma pack(push, 1)
struct IcoHeader
{
    uint16_t reserved; // Reserved. Should always be 0.
    uint16_t type;     // Specifies image type: 1 for icon (.ICO) image, 2 for cursor (.CUR) image. Other values are invalid.
    uint16_t count;    // Specifies number of images in the file.
};

// List of icons.
// Size = IcoHeader.ount * 16
struct IcoDirentry
{
    uint8_t width;    // Specifies image width in pixels. Can be 0, 255 or a number between 0 to 255. Should be 0 if image width is 256 pixels.
    uint8_t height;   // Specifies image height in pixels. Can be 0, 255 or a number between 0 to 255. Should be 0 if image height is 256 pixels.
    uint8_t colors;   // Specifies number of colors in the color palette. Should be 0 if the image is truecolor.
    uint8_t reserved; // Reserved. Should be 0.[Notes 1]
    uint16_t planes;  // In .ICO format: Specifies color planes. Should be 0 or 1.
                      // In .CUR format: Specifies the horizontal coordinates of the hotspot in number of pixels from the left.
    uint16_t bits;    // In .ICO format: Specifies bits per pixel. (1, 4, 8)
                      // In .CUR format: Specifies the vertical coordinates of the hotspot in number of pixels from the top.
    uint32_t size;    // Specifies the size of the bitmap data in bytes. Size of (InfoHeader + ANDbitmap + XORbitmap)
    uint32_t offset;  // Specifies the offset of bitmap data address in the file
};

// Variant of BMP InfoHeader.
// Size = 40 bytes.
struct IcoBmpInfoHeader
{
    uint32_t size;      // Size of InfoHeader structure = 40
    uint32_t width;     // Icon Width
    uint32_t height;    // Icon Height (added height of XOR-Bitmap and AND-Bitmap)
    uint16_t planes;    // number of planes = 1
    uint16_t bits;      // bits per pixel = 1, 2, 4, 8, 16, 24, 32
    uint32_t reserved0; // Type of Compression = 0
    uint32_t imagesize; // Size of Image in Bytes = 0 (uncompressed)
    uint32_t reserved1; // XpixelsPerM
    uint32_t reserved2; // YpixelsPerM
    uint32_t reserved3; // ColorsUsed
    uint32_t reserved4; // ColorsImportant
};

// Color Map for XOR-Bitmap.
// Size = NumberOfColors * 4 bytes.
struct IcoColors
{
    uint8_t red;      // red component
    uint8_t green;    // green component
    uint8_t blue;     // blue component
    uint8_t reserved; // = 0
};

// uint8_t xormask; // DIB bits for XOR mask
// uint8_t andmask; // DIB bits for AND mask
#pragma pack(pop)

cFormatIco::cFormatIco(iCallbacks* callbacks)
    : cFormat(callbacks)
{
}

cFormatIco::~cFormatIco()
{
}

bool cFormatIco::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, sizeof(IcoHeader)))
    {
        return false;
    }

    const auto h = reinterpret_cast<const IcoHeader*>(buffer.data());
    return (h->reserved == 0 && h->count > 0
            && h->count * sizeof(IcoDirentry) <= (size_t)file.getSize()
            && (h->type == 1 || h->type == 2));
}

bool cFormatIco::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    m_filename = filename;
    return load(0, desc);
}

bool cFormatIco::LoadSubImageImpl(uint32_t current, sBitmapDescription& desc)
{
    return load(current, desc);
}

bool cFormatIco::load(uint32_t current, sBitmapDescription& desc)
{
    cFile file;
    if (!file.open(m_filename.c_str()))
    {
        return false;
    }

    desc.size = file.getSize();

    IcoHeader header;
    if (sizeof(header) != file.read(&header, sizeof(header)))
    {
        return false;
    }

    std::vector<char> buffer(header.count * sizeof(IcoDirentry));
    auto images = reinterpret_cast<const IcoDirentry*>(buffer.data());
    if (buffer.size() != file.read(buffer.data(), buffer.size()))
    {
        return false;
    }

    current = std::max<uint32_t>(current, 0);
    current = std::min<uint32_t>(current, header.count - 1);

    const auto image = &images[current];
    // ::printf("--- IcoDirentry ---\n");
    // ::printf("(II) width: %u.\n", (uint32_t)image->width);
    // ::printf("(II) height: %u.\n", (uint32_t)image->height);
    // ::printf("(II) colors: %u.\n", (uint32_t)image->colors);
    // ::printf("(II) planes: %u.\n", (uint32_t)image->planes);
    // ::printf("(II) bits: %u.\n", (uint32_t)image->bits);
    // ::printf("(II) size: %u.\n", (uint32_t)image->size);
    // ::printf("(II) offset: %u.\n", (uint32_t)image->offset);

    bool result = false;

    if (image->colors == 0 && image->width == 0 && image->height == 0)
    {
        m_formatName = "ico/png";

        result = loadPngFrame(desc, file, image);
    }
    else
    {
        m_formatName = "ico";

        result = loadOrdinaryFrame(desc, file, image);
    }

    // store frame number and frames count after reset again
    desc.images = header.count;
    desc.current = current;

    return result;
}

bool cFormatIco::loadPngFrame(sBitmapDescription& desc, cFile& file, const IcoDirentry* image)
{
    auto size = image->size;
    std::vector<uint8_t> buffer(size);
    auto data = buffer.data();

    file.seek(image->offset, SEEK_SET);
    if (size != file.read(data, size))
    {
        ::printf("(EE) Can't read ico/png frame.\n");
        return false;
    }

    cPngReader reader;
    reader.setProgressCallback([this](float percent) {
        updateProgress(percent);
    });

    bool result = reader.loadPng(desc, data, size);

    if (result)
    {
        auto& iccProfile = reader.getIccProfile();
        if (iccProfile.size() != 0)
        {
            if (applyIccProfile(desc, iccProfile.data(), iccProfile.size()))
            {
                m_formatName = "ico/png/icc";
            }
        }
    }

    return result;
}

// load frame in ordinary format
bool cFormatIco::loadOrdinaryFrame(sBitmapDescription& desc, cFile& file, const IcoDirentry* image)
{
    file.seek(image->offset, SEEK_SET);
    std::vector<uint8_t> p(image->size);
    if (image->size != file.read(p.data(), p.size()))
    {
        ::printf("(EE) Can't read icon data.\n");
        return false;
    }

    auto imgHeader = reinterpret_cast<const IcoBmpInfoHeader*>(p.data());
    desc.width = imgHeader->width;
    desc.height = imgHeader->height / 2; // xor mask + and mask
    desc.pitch = desc.width * 4;
    desc.bpp = 32;
    desc.bppImage = imgHeader->bits;
    desc.format = GL_RGBA;

    int pitch = calcIcoPitch(desc.bppImage, desc.width);
    if (pitch == -1)
    {
        ::printf("(EE) Invalid icon pitch.\n");
        return false;
    }

    desc.bitmap.resize(desc.pitch * desc.height);
    auto out = desc.bitmap.data();

    // ::printf("--- IcoBmpInfoHeader ---\n");
    // ::printf("size: %u\n", imgHeader->size);
    // ::printf("width: %u\n", imgHeader->width);
    // ::printf("height: %u\n", imgHeader->height);
    // ::printf("planes: %u\n", (uint32_t)imgHeader->planes);
    // ::printf("bits: %u\n", (uint32_t)imgHeader->bits);
    // ::printf("imagesize: %u\n", imgHeader->imagesize);

    // const uint32_t colors = image->colors == 0 ? (1 << desc.bppImage) : image->colors;
    uint32_t colors = image->colors;
    if (desc.bppImage < 16)
    {
        colors = colors == 0 ? (1 << desc.bppImage) : image->colors;
    }
    const auto palette = reinterpret_cast<const uint32_t*>(p.data() + imgHeader->size);
    const auto xorMask = reinterpret_cast<const uint8_t*>(p.data() + imgHeader->size + colors * 4);
    const auto andMask = reinterpret_cast<const uint8_t*>(p.data() + imgHeader->size + colors * 4 + desc.height * pitch);

    switch (desc.bppImage)
    {
    case 1:
        for (uint32_t y = 0; y < desc.height; y++)
        {
            uint32_t idx = (desc.height - y - 1) * desc.pitch;
            for (uint32_t x = 0; x < desc.width; x++)
            {
                const uint32_t color = palette[getBit(xorMask, y * desc.width + x, desc.width)];

                out[idx + 0] = ((uint8_t*)(&color))[2];
                out[idx + 1] = ((uint8_t*)(&color))[1];
                out[idx + 2] = ((uint8_t*)(&color))[0];
                out[idx + 3] = getBit(andMask, y * desc.width + x, desc.width) ? 0 : 255;
                idx += 4;

                updateProgress((float)desc.height * desc.width / (y * desc.width + x));
            }
        }
        break;

    case 4:
        for (uint32_t y = 0; y < desc.height; y++)
        {
            uint32_t idx = (desc.height - y - 1) * desc.pitch;
            for (uint32_t x = 0; x < desc.width; x++)
            {
                const uint32_t color = palette[getNibble(xorMask, y * desc.width + x, desc.width)];

                out[idx + 0] = ((uint8_t*)(&color))[2];
                out[idx + 1] = ((uint8_t*)(&color))[1];
                out[idx + 2] = ((uint8_t*)(&color))[0];
                out[idx + 3] = getBit(andMask, y * desc.width + x, desc.width) ? 0 : 255;
                idx += 4;

                updateProgress((float)desc.height * desc.width / (y * desc.width + x));
            }
        }
        break;

    case 8:
        for (uint32_t y = 0; y < desc.height; y++)
        {
            uint32_t idx = (desc.height - y - 1) * desc.pitch;
            for (uint32_t x = 0; x < desc.width; x++)
            {
                const uint32_t color = palette[getByte(xorMask, y * desc.width + x, desc.width)];

                out[idx + 0] = ((uint8_t*)(&color))[2];
                out[idx + 1] = ((uint8_t*)(&color))[1];
                out[idx + 2] = ((uint8_t*)(&color))[0];
                out[idx + 3] = getBit(andMask, y * desc.width + x, desc.width) ? 0 : 255;
                idx += 4;

                updateProgress((float)desc.height * desc.width / (y * desc.width + x));
            }
        }
        break;

    default: {
        const uint32_t bpp = desc.bppImage / 8;
        for (uint32_t y = 0; y < desc.height; y++)
        {
            const uint8_t* row = xorMask + pitch * y;

            uint32_t idx = (desc.height - y - 1) * desc.pitch;
            for (uint32_t x = 0; x < desc.width; x++)
            {
                out[idx + 0] = row[2];
                out[idx + 1] = row[1];
                out[idx + 2] = row[0];

                if (desc.bppImage < 32)
                {
                    out[idx + 3] = getBit(andMask, y * desc.width + x, desc.width) ? 0 : 255;
                }
                else
                {
                    out[idx + 3] = row[3];
                }

                idx += 4;
                row += bpp;

                updateProgress((float)desc.height * desc.width / (y * desc.width + x));
            }
        }
    }
    break;
    }

    return true;
}

int cFormatIco::calcIcoPitch(uint32_t bppImage, uint32_t width)
{
    switch (bppImage)
    {
    case 1:
        if ((width % 32) == 0)
        {
            return width / 8;
        }
        return 4 * (width / 32 + 1);

    case 4:
        if ((width % 8) == 0)
        {
            return width / 2;
        }
        return 4 * (width / 8 + 1);

    case 8:
        if ((width % 4) == 0)
        {
            return width;
        }
        return 4 * (width / 4 + 1);

    case 24:
        if (((width * 3) % 4) == 0)
        {
            return width * 3;
        }
        return 4 * (width * 3 / 4 + 1);

    case 32:
        return width * 4;
    }

    ::printf("(EE) Invalid bits count: %u.\n", bppImage);
    return -1; // width * (bppImage / 8);
}

uint32_t cFormatIco::getBit(const uint8_t* data, uint32_t bit, uint32_t width)
{
    const uint32_t w = width % 32 == 0 ? (width / 32) : (width / 32 + 1);
    const uint32_t line = bit / width;
    const uint32_t offset = bit % width;

    uint32_t result = data[line * w * 4 + offset / 8] & (1 << (7 - (offset % 8)));

    return (result ? 1 : 0);
}

uint32_t cFormatIco::getNibble(const uint8_t* data, uint32_t nibble, uint32_t width)
{
    const uint32_t w = width % 8 == 0 ? (width / 8) : (width / 8 + 1);
    const uint32_t line = nibble / width;
    const uint32_t offset = nibble % width;

    uint32_t result = data[line * w * 4 + offset / 2] & (0x0F << (4 * (1 - offset % 2)));

    if (offset % 2 == 0)
    {
        result = result >> 4;
    }

    return result;
}

uint32_t cFormatIco::getByte(const uint8_t* data, uint32_t byte, uint32_t width)
{
    const uint32_t w = width % 4 == 0 ? (width / 4) : (width / 4 + 1);
    const uint32_t line = byte / width;
    const uint32_t offset = byte % width;

    return data[line * w * 4 + offset];
}

/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "formatjp2k.h"

#if defined(JPEG2000_SUPPORT)

#include "common/bitmap_description.h"
#include "common/file.h"
#include "common/helpers.h"

#include <cstring>
#include <openjpeg.h>

namespace
{
    void j2k_info_callback(const char* msg, void* /*client_data*/)
    {
        // ::printf("(II) %s", msg);
        (void)msg;
    }

    void j2k_warning_callback(const char* msg, void* /*client_data*/)
    {
        // ::printf("(WW) %s", msg);
        (void)msg;
    }

    void j2k_error_callback(const char* msg, void* /*client_data*/)
    {
        ::printf("(EE) %s", msg);
    }

    // int int_ceildivpow2(int a, int b)
    // {
    // return (a + (1 << b) - 1) >> b;
    // }

    uint8_t* getScanLine(sBitmapDescription& desc, uint32_t y)
    {
        return desc.bitmap.data() + desc.pitch * y;
    }

    typedef uint8_t* (*FillPixelFunction)(opj_image_t* image, uint32_t pixel_pos, uint8_t* bits);

    const uint32_t RGBA_RED = 0;
    const uint32_t RGBA_GREEN = 1;
    const uint32_t RGBA_BLUE = 2;
    const uint32_t RGBA_ALPHA = 3;

    uint8_t* RGBAtoRGBA(opj_image_t* image, uint32_t pixel_pos, uint8_t* bits)
    {
        uint32_t r = image->comps[0].data[pixel_pos];
        r += (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);

        uint32_t g = image->comps[1].data[pixel_pos];
        g += (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);

        uint32_t b = image->comps[2].data[pixel_pos];
        b += (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);

        uint32_t a = image->comps[3].data[pixel_pos];
        a += (image->comps[3].sgnd ? 1 << (image->comps[3].prec - 1) : 0);

        bits[RGBA_RED] = (uint8_t)r;
        bits[RGBA_GREEN] = (uint8_t)g;
        bits[RGBA_BLUE] = (uint8_t)b;
        bits[RGBA_ALPHA] = (uint8_t)a;
        bits += 4;

        return bits;
    }

    uint8_t* CMYKtoRGB(opj_image_t* image, uint32_t pixel_pos, uint8_t* bits)
    {
        uint32_t cc = image->comps[0].data[pixel_pos];
        cc += (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);
        float C = cc / 255.0f;

        uint32_t cm = image->comps[1].data[pixel_pos];
        cm += (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);
        float M = cm / 255.0f;

        uint32_t cy = image->comps[2].data[pixel_pos];
        cy += (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);
        float Y = cy / 255.0f;

        uint32_t ck = image->comps[3].data[pixel_pos];
        ck += (image->comps[3].sgnd ? 1 << (image->comps[3].prec - 1) : 0);
        float K = ck / 255.0f;

        float Kinv = 1.0f - K;

        bits[RGBA_RED] = (uint8_t)(255 * (1.0f - C) * Kinv);
        bits[RGBA_GREEN] = (uint8_t)(255 * (1.0f - M) * Kinv);
        bits[RGBA_BLUE] = (uint8_t)(255 * (1.0f - Y) * Kinv);

        bits += 3;

        return bits;
    }

    uint8_t* RGBtoRGB(opj_image_t* image, uint32_t pixel_pos, uint8_t* bits)
    {
        uint32_t r = image->comps[0].data[pixel_pos];
        r += (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);

        uint32_t g = image->comps[1].data[pixel_pos];
        g += (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);

        uint32_t b = image->comps[2].data[pixel_pos];
        b += (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);

        bits[RGBA_RED] = (uint8_t)r;
        bits[RGBA_GREEN] = (uint8_t)g;
        bits[RGBA_BLUE] = (uint8_t)b;
        bits += 3;

        return bits;
    }

#if 0
    uint8_t* SYCCtoRGB(opj_image_t* image, uint32_t pixel_pos, uint8_t* bits)
    {
        uint32_t y = image->comps[0].data[pixel_pos];
        y += (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);
        float Y = y / 255.0f;

        uint32_t cb = image->comps[1].data[pixel_pos];
        cb += (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);
        float Cb = cb / 255.0f;

        uint32_t cr = image->comps[2].data[pixel_pos];
        cr += (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);
        float Cr = cr / 255.0f;

        float r = std::max<float>(0.0f, std::min(1.0f, (float)(Y + 0.0000 * Cb + 1.4022 * Cr)));
        float g = std::max<float>(0.0f, std::min(1.0f, (float)(Y - 0.3456 * Cb - 0.7145 * Cr)));
        float b = std::max<float>(0.0f, std::min(1.0f, (float)(Y + 1.7710 * Cb + 0.0000 * Cr)));

        bits[RGBA_RED] = (uint8_t)(r * 255);
        bits[RGBA_GREEN] = (uint8_t)(g * 255);
        bits[RGBA_BLUE] = (uint8_t)(b * 255);

        bits += 3;

        return bits;
    }
#endif

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

    void allocBitmap(sBitmapDescription& desc, uint32_t bpp, GLenum format)
    {
        desc.bpp = bpp;
        desc.format = format;
        desc.pitch = helpers::calculatePitch(desc.width, desc.bpp);
        desc.bitmap.resize(desc.pitch * desc.height);
    }
} // namespace

cFormatJp2k::cFormatJp2k(iCallbacks* callbacks)
    : cFormat(callbacks)
{
}

cFormatJp2k::~cFormatJp2k()
{
}

bool cFormatJp2k::isSupported(cFile& file, Buffer& buffer) const
{
    const uint8_t jp2_signature[] = { 0x00, 0x00, 0x00, 0x0C, 0x6A, 0x50, 0x20, 0x20, 0x0D, 0x0A, 0x87, 0x0A };

    if (!readBuffer(file, buffer, sizeof(jp2_signature)))
    {
        return false;
    }

    return ::memcmp(jp2_signature, buffer.data(), sizeof(jp2_signature)) == 0;
}

size_t streamRead(void* buffer, size_t size, void* user)
{
    // ::printf("read bytes %zu\n", size);
    auto file = static_cast<cFile*>(user);
    auto readed = file->read(buffer, size);
    return readed ? readed : (size_t)-1;
}

void streamClose(void* user)
{
    // ::printf("close jpeg2k file\n");
    auto file = static_cast<cFile*>(user);
    file->close();
}

off_t streamSkip(off_t bytes, void* user)
{
    // ::printf("skip bytes %lld\n", bytes);
    auto file = static_cast<cFile*>(user);
    if (file->seek(bytes, SEEK_CUR) != 0)
    {
        return -1;
    }

    return bytes;
}

int streamSeek(off_t bytes, void* user)
{
    // ::printf("seek bytes %lld\n", bytes);
    auto file = static_cast<cFile*>(user);
    return file->seek(bytes, SEEK_SET) == 0;
}

bool cFormatJp2k::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    cFile file;
    if (!file.open(filename))
    {
        return false;
    }

    desc.size = file.getSize();

    auto stream = opj_stream_default_create(true);

    opj_stream_set_user_data(stream, &file, streamClose);
    opj_stream_set_user_data_length(stream, desc.size);

    opj_stream_set_read_function(stream, streamRead);
    opj_stream_set_skip_function(stream, streamSkip);
    opj_stream_set_seek_function(stream, streamSeek);

    opj_dparameters_t parameters;
    opj_set_default_decoder_parameters(&parameters);

    // decode the JPEG-2000 codestream

    // get a decoder handle
    auto d_codec = opj_create_decompress(OPJ_CODEC_JP2);

    // configure the event callbacks
    // catch events using our callbacks (no local context needed here)
    opj_set_info_handler(d_codec, j2k_info_callback, nullptr);
    opj_set_warning_handler(d_codec, j2k_warning_callback, nullptr);
    opj_set_error_handler(d_codec, j2k_error_callback, nullptr);

    // setup the decoder decoding parameters using user parameters
    if (!opj_setup_decoder(d_codec, &parameters))
    {
        ::printf("Failed to setup the decoder\n");
        return false;
    }

    opj_image_t* image = nullptr;

    // read the main header of the codestream and if necessary the JP2 boxes
    if (!opj_read_header(stream, d_codec, &image))
    {
        ::printf("(EE) Failed to read the header.\n");
        return false;
    }

    // decode the stream and fill the image structure
    if (!(opj_decode(d_codec, stream, image) && opj_end_decompress(d_codec, stream)))
    {
        ::printf("(EE) Failed to decode image.\n");
        return false;
    }

    // free the codec context
    opj_destroy_codec(d_codec);
    d_codec = nullptr;

    // create output image
    if (loadJp2k(image, desc) == false)
    {
        ::printf("(EE) Failed to import JPEG2000 image.\n");
        return false;
    }

    m_formatName = "jpeg2000";

    auto iccProfile = image->icc_profile_buf;
    auto iccProfileSize = image->icc_profile_len;
    if (iccProfile != nullptr && iccProfileSize != 0)
    {
        // uint32_t numcomps = image->numcomps;

        if (applyIccProfile(desc, iccProfile, iccProfileSize))
        {
            m_formatName = "jpeg2000/icc";
        }
    }

    // free image data structure
    opj_image_destroy(image);

    opj_stream_destroy(stream);

    return true;
}

bool cFormatJp2k::loadJp2k(void* img, sBitmapDescription& desc) const
{
    auto image = static_cast<opj_image_t*>(img);

    // compute image width and height

    uint32_t wr = image->comps[0].w;
    // uint32_t wrr = int_ceildivpow2(image->comps[0].w, image->comps[0].factor);
    // uint32_t hrr = int_ceildivpow2(image->comps[0].h, image->comps[0].factor);

    // check the number of components

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

    desc.width = image->comps[0].w;
    desc.height = image->comps[0].h;

    desc.bppImage = numcomps * image->comps[0].prec;

    desc.images = 1;

    if (image->comps[0].prec > 16)
    {
        return false; // ERROR_UNSUPPORTED_FORMAT;
    }

    ::printf("\n");
    ::printf("Components: %u\n", numcomps);
    ::printf("  Colorspace: %s\n", getColorSpaceName(colorspace));
    ::printf("  Prec: %u\n", image->comps[0].prec);
    ::printf("  Signed: %u\n", image->comps[0].sgnd);
    ::printf("  Factor: %u\n", image->comps[0].factor);
    ::printf("  Decoded resolution: %u\n", image->comps[0].resno_decoded);

    if (image->comps[0].prec <= 8)
    {
        if (numcomps == 1)
        {
            // 8-bit greyscale
            // ----------------------------------------------------------

            // load pixel data
            allocBitmap(desc, 8, GL_LUMINANCE);

            uint32_t pixel_count = 0;

            for (uint32_t y = 0; y < desc.height; y++)
            {
                uint8_t* bits = getScanLine(desc, y);

                for (uint32_t x = 0; x < desc.width; x++)
                {
                    const uint32_t pixel_pos = pixel_count / desc.width * wr + pixel_count % desc.width;

                    uint32_t value = image->comps[0].data[pixel_pos];
                    value += (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);

                    bits[x] = (uint8_t)value;

                    pixel_count++;
                }

                updateProgress(static_cast<float>(y) / desc.height);
            }
        }
        else if (numcomps == 2)
        {
            // 8-bit greyscale
            // ----------------------------------------------------------

            // load pixel data
            allocBitmap(desc, 8, GL_LUMINANCE);

            uint32_t pixel_count = 0;

            for (uint32_t y = 0; y < desc.height; y++)
            {
                uint8_t* bits = getScanLine(desc, y);

                for (uint32_t x = 0; x < desc.width; x++)
                {
                    const uint32_t pixel_pos = pixel_count / desc.width * wr + pixel_count % desc.width;

                    uint32_t value = image->comps[1].data[pixel_pos];
                    value += (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);

                    bits[x] = (uint8_t)value;

                    pixel_count++;
                }

                updateProgress(static_cast<float>(y) / desc.height);
            }
        }
        else if (numcomps == 3)
        {
            // 24-bit RGB
            // ----------------------------------------------------------

            // load pixel data
            uint32_t pixel_count = 0;

            allocBitmap(desc, 24, GL_RGB);

            for (uint32_t y = 0; y < desc.height; y++)
            {
                uint8_t* bits = getScanLine(desc, y);

                for (uint32_t x = 0; x < desc.width; x++)
                {
                    const uint32_t pixel_pos = pixel_count / desc.width * wr + pixel_count % desc.width;
                    bits = RGBtoRGB(image, pixel_pos, bits);

                    pixel_count++;
                }

                updateProgress(static_cast<float>(y) / desc.height);
            }
        }
        else if (numcomps >= 4)
        {
            // 32-bit RGBA
            // ----------------------------------------------------------

            // load pixel data

            uint32_t pixel_count = 0;

            FillPixelFunction fillPixel = nullptr;
            if (colorspace == OPJ_CLRSPC_CMYK)
            {
                allocBitmap(desc, 24, GL_RGB);
                fillPixel = CMYKtoRGB;
            }
            else
            {
                allocBitmap(desc, 32, GL_RGBA);
                fillPixel = RGBAtoRGBA;
            }

            for (uint32_t y = 0; y < desc.height; y++)
            {
                uint8_t* bits = getScanLine(desc, y);

                for (uint32_t x = 0; x < desc.width; x++)
                {
                    const uint32_t pixel_pos = pixel_count / desc.width * wr + pixel_count % desc.width;
                    bits = fillPixel(image, pixel_pos, bits);

                    pixel_count++;
                }

                updateProgress(static_cast<float>(y) / desc.height);
            }
        }
    }
    else if (image->comps[0].prec <= 16)
    {
        if (numcomps == 1)
        {
            // 16-bit greyscale
            // ----------------------------------------------------------

            // load pixel data
            allocBitmap(desc, 8, GL_LUMINANCE);

            uint32_t pixel_count = 0;

            for (uint32_t y = 0; y < desc.height; y++)
            {
                auto bits = (uint8_t*)getScanLine(desc, y);

                for (uint32_t x = 0; x < desc.width; x++)
                {
                    const uint32_t pixel_pos = pixel_count / desc.width * wr + pixel_count % desc.width;

                    uint32_t value = image->comps[0].data[pixel_pos];
                    value += (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);

                    bits[x] = (uint8_t)(value >> 4);

                    pixel_count++;
                }

                updateProgress(static_cast<float>(y) / desc.height);
            }
        }
        else if (numcomps == 3)
        {
            // 48-bit RGB
            // ----------------------------------------------------------

            // load pixel data
            allocBitmap(desc, 24, GL_RGB);

            uint32_t pixel_count = 0;

            for (uint32_t y = 0; y < desc.height; y++)
            {
                auto bits = (uint8_t*)getScanLine(desc, y);

                for (uint32_t x = 0; x < desc.width; x++)
                {
                    const uint32_t pixel_pos = pixel_count / desc.width * wr + pixel_count % desc.width;

                    uint32_t r = image->comps[0].data[pixel_pos];
                    r += (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);

                    uint32_t g = image->comps[1].data[pixel_pos];
                    g += (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);

                    uint32_t b = image->comps[2].data[pixel_pos];
                    b += (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);

                    bits[RGBA_RED] = (uint8_t)(r >> 8);
                    bits[RGBA_GREEN] = (uint8_t)(g >> 8);
                    bits[RGBA_BLUE] = (uint8_t)(b >> 8);
                    bits += 3;

                    pixel_count++;
                }

                updateProgress(static_cast<float>(y) / desc.height);
            }
        }
        else if (numcomps == 4)
        {
            // 64-bit RGBA
            // ----------------------------------------------------------

            // load pixel data
            allocBitmap(desc, 32, GL_RGBA);

            uint32_t pixel_count = 0;

            for (uint32_t y = 0; y < desc.height; y++)
            {
                auto bits = (uint8_t*)getScanLine(desc, y); // desc.height - 1 - y);

                for (uint32_t x = 0; x < desc.width; x++)
                {
                    const uint32_t pixel_pos = pixel_count / desc.width * wr + pixel_count % desc.width;

                    uint32_t r = image->comps[0].data[pixel_pos];
                    r += (image->comps[0].sgnd ? 1 << (image->comps[0].prec - 1) : 0);

                    uint32_t g = image->comps[1].data[pixel_pos];
                    g += (image->comps[1].sgnd ? 1 << (image->comps[1].prec - 1) : 0);

                    uint32_t b = image->comps[2].data[pixel_pos];
                    b += (image->comps[2].sgnd ? 1 << (image->comps[2].prec - 1) : 0);

                    uint32_t a = image->comps[3].data[pixel_pos];
                    a += (image->comps[3].sgnd ? 1 << (image->comps[3].prec - 1) : 0);

                    bits[RGBA_RED] = (uint8_t)r;
                    bits[RGBA_GREEN] = (uint8_t)g;
                    bits[RGBA_BLUE] = (uint8_t)b;
                    bits[RGBA_ALPHA] = (uint8_t)a;
                    bits += 4;

                    pixel_count++;
                }

                updateProgress(static_cast<float>(y) / desc.height);
            }
        }
    }

    return true;
}

#endif

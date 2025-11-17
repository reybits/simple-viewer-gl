/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "formatsvg.h"
#include "common/bitmap_description.h"
#include "common/config.h"
#include "common/file.h"

#include <cfloat>
#include <cmath>
#include <cstring>

#define NANOSVG_ALL_COLOR_KEYWORDS
#define NANOSVG_IMPLEMENTATION
#include "libs/nanosvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "libs/nanosvgrast.h"

cFormatSvg::cFormatSvg(iCallbacks* callbacks)
    : cFormat(callbacks)
{
    m_rasterizer = nsvgCreateRasterizer();
}

cFormatSvg::~cFormatSvg()
{
    nsvgDeleteRasterizer(m_rasterizer);
}

bool cFormatSvg::isSupported(cFile& file, Buffer& buffer) const
{
    auto len = std::min<uint32_t>(file.getSize(), 4096);

    if (!readBuffer(file, buffer, len))
    {
        return false;
    }

    auto magic = (char*)buffer.data();
    magic[len] = '\0';

    return ::strstr(magic, "<svg") != nullptr;
}

bool cFormatSvg::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    if (m_rasterizer == nullptr)
    {
        ::printf("(EE) SVG rasterizer isn't created.\n");
        return false;
    }

    cFile file;
    if (file.open(filename) == false)
    {
        ::printf("(EE) Couldn't open file.\n");
        return false;
    }

    std::vector<char> data(file.getSize());
    if (file.read(data.data(), file.getSize()) != file.getSize())
    {
        ::printf("(EE) Couldn't read file.\n");
        return false;
    }

    auto image = nsvgParse(data.data(), "px", 96.0f);
    if (image == nullptr)
    {
        ::printf("(EE) Couldn't parse SVG image.\n");
        return false;
    }

    auto scale = 1.0f;

    const auto minSize = m_config->minSvgSize;
    // ::printf("Config SVG size: %.1f\n", minSize);

    if (image->width < minSize && image->height < minSize)
    {
        const auto sw = minSize / image->width;
        const auto sh = minSize / image->height;
        scale = std::min(sw, sh);
        ::printf("SVG size to small, upscale to %.1f x %.1f\n", image->width * scale, image->width * scale);
        ::printf("Calculated scale: %.1f x %.1f\n", sw, sh);
    }

    // ::printf("Selected scale: %.1f\n", scale);

    desc.size = file.getSize();
    desc.images = 1;
    desc.format = GL_RGBA;
    desc.bpp = 32;
    desc.bppImage = 32;
    desc.width = image->width * scale;
    desc.height = image->height * scale;
    desc.pitch = desc.width * 4;
    desc.bitmap.resize(desc.pitch * desc.height);
    auto pix = desc.bitmap.data();
    // std::fill(desc.bitmap.begin(), desc.bitmap.end(), 0);

    m_formatName = "svg";

    nsvgRasterize(m_rasterizer, image, 0.0f, 0.0f, scale, pix, desc.width, desc.height, desc.pitch);
    nsvgDelete(image);

    return true;
}

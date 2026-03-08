/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatSvg.h"
#include "Common/BitmapDescription.h"
#include "Common/Config.h"
#include "Common/File.h"
#include "Log/Log.h"

#include <cfloat>
#include <cmath>
#include <cstring>

#define NANOSVG_ALL_COLOR_KEYWORDS
#define NANOSVG_IMPLEMENTATION
#include "Libs/NanoSvg.h"

#define NANOSVGRAST_IMPLEMENTATION
#include "Libs/NanoSvgRast.h"

cFormatSvg::cFormatSvg(sCallbacks* callbacks)
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
        cLog::Error("Can't create SVG rasterizer.");
        return false;
    }

    cFile file;
    if (!openFile(file, filename, desc))
    {
        cLog::Error("Can't open SVG file.");
        return false;
    }

    std::vector<char> data(file.getSize());
    if (file.read(data.data(), file.getSize()) != file.getSize())
    {
        cLog::Error("Can't read SVG file.");
        return false;
    }

    auto image = nsvgParse(data.data(), "px", 96.0f);
    if (image == nullptr)
    {
        cLog::Error("Can't parse SVG image.");
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
        cLog::Info("SVG size too small, upscaling to {} x {}.", image->width * scale, image->width * scale);
        cLog::Info("Calculated scale: {} x {}.", sw, sh);
    }

    // ::printf("Selected scale: %.1f\n", scale);

    desc.images = 1;
    desc.format = ePixelFormat::RGBA;
    desc.bpp = 32;
    desc.bppImage = 32;
    desc.width = image->width * scale;
    desc.height = image->height * scale;
    desc.allocate(desc.width, desc.height, 32, ePixelFormat::RGBA);
    auto pix = desc.bitmap.data();
    // std::fill(desc.bitmap.begin(), desc.bitmap.end(), 0);

    desc.formatName = "svg";

    nsvgRasterize(m_rasterizer, image, 0.0f, 0.0f, scale, pix, desc.width, desc.height, desc.pitch);
    nsvgDelete(image);

    return true;
}

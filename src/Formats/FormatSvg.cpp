/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatSvg.h"
#include "Common/ChunkData.h"
#include "Common/Config.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
#include "Log/Log.h"

#include <cstring>
#include <lunasvg.h>

cFormatSvg::cFormatSvg(sCallbacks* callbacks)
    : cFormat(callbacks)
{
}

cFormatSvg::~cFormatSvg()
{
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

bool cFormatSvg::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (!openFile(file, filename, info))
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

    auto document = lunasvg::Document::loadFromData(data.data(), data.size());
    if (document == nullptr)
    {
        // Try extracting embedded <svg>...</svg> block (e.g. from HTML files).
        auto svgStart = helpers::memfind(data.data(), data.size(), "<svg");
        auto svgEnd = helpers::memfind(data.data(), data.size(), "</svg>");
        if (svgStart != nullptr && svgEnd != nullptr && svgEnd > svgStart)
        {
            svgEnd += 6; // include "</svg>"
            document = lunasvg::Document::loadFromData(svgStart, svgEnd - svgStart);
        }
        if (document == nullptr)
        {
            cLog::Error("Can't parse SVG document.");
            return false;
        }
    }

    const auto svgWidth = document->width();
    const auto svgHeight = document->height();
    if (svgWidth <= 0.0f || svgHeight <= 0.0f)
    {
        cLog::Error("Invalid SVG dimensions: {} x {}.", svgWidth, svgHeight);
        return false;
    }

    auto scale = 1.0f;
    const auto minSize = m_config->minSvgSize;
    cLog::Debug("Config SVG size: {:.1f}.", minSize);

    if (svgWidth < minSize && svgHeight < minSize)
    {
        const auto sw = minSize / svgWidth;
        const auto sh = minSize / svgHeight;
        scale = std::min(sw, sh);
        cLog::Info("SVG size too small, upscaling to {} x {}.", svgWidth * scale, svgHeight * scale);
        cLog::Info("Calculated scale: {} x {}.", sw, sh);
    }

    cLog::Debug("Selected scale: {:.1f}.", scale);

    const auto width = static_cast<int>(svgWidth * scale);
    const auto height = static_cast<int>(svgHeight * scale);

    auto bitmap = document->renderToBitmap(width, height);
    if (bitmap.isNull())
    {
        cLog::Error("Can't rasterize SVG document.");
        return false;
    }

    info.images = 1;
    chunk.format = ePixelFormat::BGRA;
    chunk.bpp = 32;
    chunk.effects = eEffect::Unpremultiply;
    info.bppImage = 32;
    chunk.width = bitmap.width();
    chunk.height = bitmap.height();
    chunk.pitch = bitmap.stride();
    chunk.bitmap.assign(bitmap.data(), bitmap.data() + chunk.pitch * chunk.height);

    info.formatName = "svg";

    return true;
}

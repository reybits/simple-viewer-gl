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

namespace
{
    struct FontEntry
    {
        const char* path;
        bool bold;
        bool italic;
    };

    void registerFallbackFonts()
    {
        static auto isRegistered = false;
        if (isRegistered)
        {
            return;
        }
        isRegistered = true;

        // lunasvg doesn't do per-glyph font fallback: once a font face is
        // selected for a text run, missing glyphs render as .notdef squares.
        // Register multiple fallback fonts (empty family) to improve coverage
        // when the SVG doesn't specify a family or the family isn't found.
        constexpr FontEntry candidates[] = {
#if defined(__APPLE__)
            // Latin
            { "/System/Library/Fonts/Helvetica.ttc", false, false },
            { "/Library/Fonts/Arial.ttf", false, false },
            { "/Library/Fonts/Arial Bold.ttf", true, false },
            { "/Library/Fonts/Arial Italic.ttf", false, true },
            { "/Library/Fonts/Arial Bold Italic.ttf", true, true },
            // CJK
            { "/System/Library/Fonts/ヒラギノ角ゴシック W3.ttc", false, false },
            { "/Library/Fonts/Arial Unicode.ttf", false, false },
            // Symbols / Emoji
            { "/System/Library/Fonts/Apple Symbols.ttf", false, false },
            { "/System/Library/Fonts/Apple Color Emoji.ttc", false, false },
#else
            // Latin
            { "/usr/share/fonts/liberation/LiberationSans-Regular.ttf", false, false },
            { "/usr/share/fonts/TTF/LiberationSans-Regular.ttf", false, false },
            { "/usr/share/fonts/liberation/LiberationSans-Bold.ttf", true, false },
            { "/usr/share/fonts/TTF/LiberationSans-Bold.ttf", true, false },
            { "/usr/share/fonts/liberation/LiberationSans-Italic.ttf", false, true },
            { "/usr/share/fonts/TTF/LiberationSans-Italic.ttf", false, true },
            { "/usr/share/fonts/noto/NotoSans-Regular.ttf", false, false },
            { "/usr/share/fonts/TTF/DejaVuSans.ttf", false, false },
            { "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", false, false },
            // CJK
            { "/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc", false, false },
            { "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc", false, false },
            // Symbols / Emoji
            { "/usr/share/fonts/noto/NotoSansSymbols2-Regular.ttf", false, false },
            { "/usr/share/fonts/google-noto-emoji/NotoColorEmoji.ttf", false, false },
            { "/usr/share/fonts/truetype/noto/NotoColorEmoji.ttf", false, false },
#endif
        };

        for (const auto& entry : candidates)
        {
            lunasvg_add_font_face_from_file("", entry.bold, entry.italic, entry.path);
        }
    }

} // namespace

cFormatSvg::cFormatSvg(sCallbacks* callbacks)
    : cFormat(callbacks)
{
    registerFallbackFonts();
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
        auto svgEnd   = helpers::memfind(data.data(), data.size(), "</svg>");
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

    m_svgWidth  = document->width();
    m_svgHeight = document->height();
    if (m_svgWidth <= 0.0f || m_svgHeight <= 0.0f)
    {
        cLog::Error("Invalid SVG dimensions: {} x {}.", m_svgWidth, m_svgHeight);
        return false;
    }

    m_document = std::move(document);

    auto scale         = 1.0f;
    const auto minSize = m_config->minSvgSize;
    cLog::Debug("Config SVG size: {:.1f}.", minSize);

    if (m_svgWidth < minSize && m_svgHeight < minSize)
    {
        const auto sw = minSize / m_svgWidth;
        const auto sh = minSize / m_svgHeight;
        scale         = std::min(sw, sh);
        cLog::Info("SVG size too small, upscaling to {} x {}.", m_svgWidth * scale, m_svgHeight * scale);
        cLog::Info("Calculated scale: {} x {}.", sw, sh);
    }

    cLog::Debug("Selected scale: {:.1f}.", scale);

    const auto width  = static_cast<uint32_t>(m_svgWidth * scale);
    const auto height = static_cast<uint32_t>(m_svgHeight * scale);

    return rasterize(width, height, chunk, info);
}

bool cFormatSvg::LoadSubImageImpl(uint32_t /*subImage*/, sChunkData& chunk, sImageInfo& info)
{
    if (m_document == nullptr)
    {
        return false;
    }

    const auto width  = m_targetWidth;
    const auto height = m_targetHeight;
    if (width == 0 || height == 0)
    {
        return false;
    }

    cLog::Debug("SVG re-rasterize: {} x {}.", width, height);

    return rasterize(width, height, chunk, info);
}

bool cFormatSvg::rasterize(uint32_t width, uint32_t height, sChunkData& chunk, sImageInfo& info)
{
    auto bitmap = m_document->renderToBitmap(static_cast<int>(width), static_cast<int>(height));
    if (bitmap.isNull())
    {
        cLog::Error("Can't rasterize SVG document.");
        return false;
    }

    info.images   = 1;
    info.isVector = true;
    chunk.format  = ePixelFormat::BGRA;
    chunk.bpp     = 32;
    chunk.effects = eEffect::Unpremultiply;
    info.bppImage = 32;
    chunk.width   = bitmap.width();
    chunk.height  = bitmap.height();
    chunk.pitch   = bitmap.stride();
    chunk.bitmap.assign(bitmap.data(), bitmap.data() + chunk.pitch * chunk.height);

    info.formatName = "svg";

    return true;
}

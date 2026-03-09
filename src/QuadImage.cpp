/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "QuadImage.h"
#include "Common/Helpers.h"
#include "Log/Log.h"
#include "Quad.h"

#include <cassert>
#include <cstring>

cQuadImage::cQuadImage()
{
}

cQuadImage::~cQuadImage()
{
    clear();
}

void cQuadImage::clear()
{
    m_compressed = false;
    m_compressedFormat = 0;
    m_compressedSize = 0;

    m_texWidth = 0;
    m_texHeight = 0;
    m_texPitch = 0;
    m_cols = 0;
    m_rows = 0;

    m_width = 0;
    m_height = 0;
    m_pitch = 0;
    m_format = ePixelFormat::RGB;
    m_bitsPerPixel = 0;
    m_image = nullptr;

    clearOld();
    m_chunks.clear();

    m_buffer.resize(0);

    m_pixelCache.x = std::numeric_limits<uint32_t>::max();
    m_pixelCache.y = std::numeric_limits<uint32_t>::max();
}

void cQuadImage::clearOld()
{
    m_chunksOld.clear();
}

void cQuadImage::setBuffer(uint32_t width, uint32_t height, uint32_t pitch, ePixelFormat format, uint32_t bpp, const uint8_t* image)
{
    m_texWidth = render::calculateTextureSize(width);
    m_texHeight = render::calculateTextureSize(height);

    m_texPitch = helpers::calculatePitch(m_texWidth, bpp);

    m_cols = (width + m_texWidth - 1) / m_texWidth;
    m_rows = (height + m_texHeight - 1) / m_texHeight;

    clearOld();
    m_chunks.clear();

    m_width = width;
    m_height = height;
    m_pitch = pitch;
    m_format = format;
    m_bitsPerPixel = bpp;
    m_image = image;

    m_buffer.resize(m_texPitch * m_texHeight);

    m_pixelCache.x = std::numeric_limits<uint32_t>::max();
    m_pixelCache.y = std::numeric_limits<uint32_t>::max();

    m_started = true;
}

void cQuadImage::refreshData(const uint8_t* image)
{
    m_image = image;

    // Keep existing chunks visible as background while re-uploading
    moveToOld();

    m_buffer.resize(m_texPitch * m_texHeight);

    m_pixelCache.x = std::numeric_limits<uint32_t>::max();
    m_pixelCache.y = std::numeric_limits<uint32_t>::max();

    m_started = true;
}

void cQuadImage::setCompressedBuffer(uint32_t width, uint32_t height, uint32_t format, uint32_t compressedSize, const uint8_t* image)
{
    moveToOld();

    m_pixelCache.x = std::numeric_limits<uint32_t>::max();
    m_pixelCache.y = std::numeric_limits<uint32_t>::max();

    m_compressed = true;
    m_compressedFormat = format;
    m_compressedSize = compressedSize;

    m_texWidth = width;
    m_texHeight = height;
    m_texPitch = 0;
    m_cols = 1;
    m_rows = 1;

    m_width = width;
    m_height = height;
    m_pitch = 0;
    m_format = ePixelFormat::RGBA;
    m_bitsPerPixel = 0;
    m_image = image;

    m_started = true;
}

uint32_t cQuadImage::getChunkHeight(uint32_t row) const
{
    return (row < m_rows - 1)
        ? m_texHeight
        : (m_height - m_texHeight * (m_rows - 1));
}

uint32_t cQuadImage::getChunkWidth(uint32_t col) const
{
    return (col < m_cols - 1)
        ? m_texWidth
        : (m_width - m_texWidth * (m_cols - 1));
}

void cQuadImage::createChunk(uint32_t col, uint32_t row, uint32_t readyHeight)
{
    const uint32_t chunkTop = row * m_texHeight;
    const uint32_t chunkH = getChunkHeight(row);
    const uint32_t w = getChunkWidth(col);
    const uint32_t available = std::min(readyHeight - chunkTop, chunkH);

    const uint32_t sx = col * m_texPitch;
    const uint32_t dstPitch = helpers::calculatePitch(w, m_bitsPerPixel);

    // Zero-fill for the full chunk, then copy available rows
    ::memset(m_buffer.data(), 0, dstPitch * chunkH);

    auto out = m_buffer.data();
    const auto in = m_image;

    for (uint32_t y = 0; y < available; y++)
    {
        const auto src = static_cast<size_t>(sx) + static_cast<size_t>(chunkTop + y) * m_pitch;
        if (src + dstPitch <= static_cast<size_t>(m_pitch) * m_height)
        {
            const uint32_t dst = y * dstPitch;
            ::memcpy(out + dst, in + src, dstPitch);
        }
    }

    cQuad* quad = findAndRemoveOld(col, row);
    if (quad == nullptr
        || quad->getTexWidth() != w || quad->getTexHeight() != chunkH
        || quad->getFormat() != m_format)
    {
        auto newQuad = std::make_unique<cQuad>(w, chunkH, out, m_format);
        newQuad->useFilter(m_filter);
        if (available < chunkH)
        {
            newQuad->setTextureRect({ 0.0f, 0.0f }, { static_cast<float>(w), static_cast<float>(available) });
        }
        m_chunks.push_back({ col, row, available, std::move(newQuad) });
    }
    else
    {
        quad->setData(out);
        quad->useFilter(m_filter);
        if (available < chunkH)
        {
            quad->setTextureRect({ 0.0f, 0.0f }, { static_cast<float>(w), static_cast<float>(available) });
        }
        else
        {
            quad->setSpriteSize({ static_cast<float>(w), static_cast<float>(chunkH) });
        }
        m_chunks.push_back({ col, row, available, std::unique_ptr<cQuad>(quad) });
    }
}

void cQuadImage::updateChunkSubData(Chunk& chunk, uint32_t available)
{
    const uint32_t w = getChunkWidth(chunk.col);
    const uint32_t newRows = available - chunk.uploadedHeight;
    const uint32_t sx = chunk.col * m_texPitch;
    const uint32_t sy = chunk.row * m_texHeight + chunk.uploadedHeight;
    const uint32_t dstPitch = helpers::calculatePitch(w, m_bitsPerPixel);

    auto out = m_buffer.data();
    const auto in = m_image;

    for (uint32_t y = 0; y < newRows; y++)
    {
        const auto src = static_cast<size_t>(sx) + static_cast<size_t>(sy + y) * m_pitch;
        if (src + dstPitch <= static_cast<size_t>(m_pitch) * m_height)
        {
            const uint32_t dst = y * dstPitch;
            ::memcpy(out + dst, in + src, dstPitch);
        }
    }

    chunk.quad->updateSubData(out, chunk.uploadedHeight, newRows);
    chunk.uploadedHeight = available;

    const uint32_t chunkH = getChunkHeight(chunk.row);
    const auto fw = static_cast<float>(w);
    if (available < chunkH)
    {
        chunk.quad->setTextureRect({ 0.0f, 0.0f }, { fw, static_cast<float>(available) });
    }
    else
    {
        chunk.quad->setSpriteSize({ fw, static_cast<float>(chunkH) });
    }
}

bool cQuadImage::upload(uint32_t readyHeight)
{
    if (m_compressed)
    {
        clearOld();

        auto quad = std::make_unique<cQuad>(m_width, m_height, m_image, static_cast<GLenum>(m_compressedFormat), m_compressedSize);
        quad->useFilter(m_filter);
        m_chunks.push_back({ 0, 0, 0, std::move(quad) });

        m_started = false;
        return true;
    }

    const auto totalChunks = m_rows * m_cols;

    // Phase 1: Update partially uploaded existing chunks with new rows
    for (auto& chunk : m_chunks)
    {
        const uint32_t chunkTop = chunk.row * m_texHeight;
        const uint32_t chunkH = getChunkHeight(chunk.row);

        if (chunk.uploadedHeight >= chunkH)
        {
            continue;
        }

        const uint32_t available = (readyHeight > chunkTop)
            ? std::min(readyHeight - chunkTop, chunkH)
            : 0;

        if (available > chunk.uploadedHeight)
        {
            updateChunkSubData(chunk, available);
        }
    }

    // Phase 2: Create new chunks when data becomes available
    while (m_chunks.size() < totalChunks)
    {
        const auto idx = m_chunks.size();
        const uint32_t col = idx % m_cols;
        const uint32_t row = idx / m_cols;
        const uint32_t chunkTop = row * m_texHeight;

        if (readyHeight <= chunkTop)
        {
            break;
        }

        createChunk(col, row, readyHeight);
    }

    const bool isDone = isUploading() == false;
    if (isDone)
    {
        stop();
    }

    return isDone;
}

void cQuadImage::stop()
{
    clearOld();
    m_buffer.resize(0);
    m_started = false;
}

bool cQuadImage::isUploading() const
{
    if (m_started == false)
    {
        return false;
    }

    if (m_chunks.size() < m_rows * m_cols)
    {
        return true;
    }

    for (const auto& chunk : m_chunks)
    {
        if (chunk.uploadedHeight < getChunkHeight(chunk.row))
        {
            return true;
        }
    }

    return false;
}

float cQuadImage::getProgress() const
{
    if (m_height == 0)
    {
        return 0.0f;
    }

    uint32_t totalUploaded = 0;
    for (const auto& chunk : m_chunks)
    {
        totalUploaded += chunk.uploadedHeight;
    }

    uint32_t totalNeeded = 0;
    for (uint32_t row = 0; row < m_rows; row++)
    {
        totalNeeded += getChunkHeight(row) * m_cols;
    }

    return (totalNeeded > 0)
        ? static_cast<float>(totalUploaded) / totalNeeded
        : 0.0f;
}

void cQuadImage::useFilter(bool filter)
{
    if (m_filter == filter)
    {
        return;
    }

    m_filter = filter;
    for (auto& chunk : m_chunks)
    {
        chunk.quad->useFilter(filter);
    }
}

bool cQuadImage::isInsideViewport(const Chunk& chunk, const Vectorf& pos) const
{
    auto& rc = render::getRect();
    const auto& size = chunk.quad->getSize();
    const Rectf rcQuad{ pos, pos + size };
    return rc.intersect(rcQuad);
}

void cQuadImage::render()
{
    const float halfWidth = static_cast<float>((m_width + 1) >> 1);
    const float halfHeight = static_cast<float>((m_height + 1) >> 1);
    const float texWidth = m_texWidth;
    const float texHeight = m_texHeight;

    bool isInside = render::getAngle() != 0;

    for (const auto& chunk : m_chunksOld)
    {
        const Vectorf pos{
            chunk.col * texWidth - halfWidth,
            chunk.row * texHeight - halfHeight
        };
        if (isInside || isInsideViewport(chunk, pos))
        {
            chunk.quad->render(pos);
        }
    }

    for (const auto& chunk : m_chunks)
    {
        const Vectorf pos{
            chunk.col * texWidth - halfWidth,
            chunk.row * texHeight - halfHeight
        };
        if (isInside || isInsideViewport(chunk, pos))
        {
            chunk.quad->render(pos);
        }
    }
}

bool cQuadImage::getPixel(uint32_t x, uint32_t y, cColor& color) const
{
    if (m_chunks.empty() || x >= m_width || y >= m_height)
    {
        return false;
    }

    // Return cached pixel if coordinates match
    if (m_pixelCache.x == x && m_pixelCache.y == y)
    {
        color.r = m_pixelCache.rgba[0];
        color.g = m_pixelCache.rgba[1];
        color.b = m_pixelCache.rgba[2];
        color.a = m_pixelCache.rgba[3];
        return true;
    }

    const uint32_t col = x / m_texWidth;
    const uint32_t row = y / m_texHeight;

    // Find the chunk
    const cQuad* quad = nullptr;
    for (const auto& chunk : m_chunks)
    {
        if (chunk.col == col && chunk.row == row)
        {
            quad = chunk.quad.get();
            break;
        }
    }

    if (quad == nullptr)
    {
        return false;
    }

    // Read a single pixel from the texture via FBO
    const uint32_t lx = x - col * m_texWidth;
    const uint32_t ly = y - row * m_texHeight;
    uint8_t rgba[4] = {};
    render::readTexPixel(quad->getQuad().tex, lx, ly, rgba);

    // FBO readback returns raw stored channels without applying texture swizzle.
    // Apply the same swizzle logic as render::setData() to get correct RGBA.
    if (m_format == ePixelFormat::Luminance)
    {
        // GL_R8 with swizzle {R,R,R,1}
        color.r = rgba[0];
        color.g = rgba[0];
        color.b = rgba[0];
        color.a = 255;
    }
    else if (m_format == ePixelFormat::LuminanceAlpha)
    {
        // GL_RG8 with swizzle {R,R,R,G}
        color.r = rgba[0];
        color.g = rgba[0];
        color.b = rgba[0];
        color.a = rgba[1];
    }
    else if (m_format == ePixelFormat::Alpha)
    {
        // GL_R8 with swizzle {0,0,0,R}
        color.r = 0;
        color.g = 0;
        color.b = 0;
        color.a = rgba[0];
    }
    else
    {
        color.r = rgba[0];
        color.g = rgba[1];
        color.b = rgba[2];
        color.a = rgba[3];
    }

    // Cache this pixel
    m_pixelCache.x = x;
    m_pixelCache.y = y;
    m_pixelCache.rgba[0] = color.r;
    m_pixelCache.rgba[1] = color.g;
    m_pixelCache.rgba[2] = color.b;
    m_pixelCache.rgba[3] = color.a;

    return true;
}

void cQuadImage::moveToOld()
{
    clearOld();

    for (size_t i = 0, size = m_chunks.size(); i < size; i++)
    {
        auto idx = size - i - 1;
        auto& chunk = m_chunks[idx];
        if (chunk.col >= m_cols || chunk.row >= m_rows)
        {
            m_chunks[idx] = std::move(m_chunks.back());
            m_chunks.pop_back();
        }
    }

    m_chunksOld = std::move(m_chunks);
    m_chunks.clear();
}

cQuad* cQuadImage::findAndRemoveOld(uint32_t col, uint32_t row)
{
    cQuad* quad = nullptr;

    for (size_t i = 0, size = m_chunksOld.size(); i < size; i++)
    {
        auto idx = size - i - 1;
        auto& chunk = m_chunksOld[idx];
        if (chunk.col == col && chunk.row == row)
        {
            quad = chunk.quad.release();
            m_chunksOld[idx] = std::move(m_chunksOld.back());
            m_chunksOld.pop_back();
            break;
        }
    }

    return quad;
}

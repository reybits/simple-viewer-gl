/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Common/PixelFormat.h"
#include "Types/Color.h"
#include "Types/Vector.h"

#include <memory>
#include <vector>

class cQuad;

class cQuadImage final
{
public:
    cQuadImage();
    ~cQuadImage();

    void clear();
    void setBuffer(uint32_t width, uint32_t height, uint32_t pitch, ePixelFormat format, uint32_t bpp, const uint8_t* image);
    void refreshData(const uint8_t* image);
    void setCompressedBuffer(uint32_t width, uint32_t height, uint32_t format, uint32_t compressedSize, const uint8_t* image);
    bool upload(uint32_t readyHeight);

    void stop();
    bool isUploading() const;
    float getProgress() const;

    void useFilter(bool filter);
    void render();

    bool getPixel(uint32_t x, uint32_t y, cColor& color) const;

    uint32_t getWidth() const
    {
        return m_width;
    }

    uint32_t getHeight() const
    {
        return m_height;
    }

    uint32_t getTexWidth() const
    {
        return m_texWidth;
    }

    uint32_t getTexHeight() const
    {
        return m_texHeight;
    }

    uint32_t getCols() const
    {
        return m_cols;
    }

    uint32_t getRows() const
    {
        return m_rows;
    }

private:
    void moveToOld();
    void clearOld();
    cQuad* findAndRemoveOld(uint32_t col, uint32_t row);

    struct sChunk
    {
        uint32_t col;
        uint32_t row;
        std::unique_ptr<cQuad> quad;
    };

    bool isInsideViewport(const sChunk& chunk, const Vectorf& pos) const;

private:
    bool m_started = false;
    bool m_filter = false;
    bool m_compressed = false;
    uint32_t m_compressedFormat = 0; // GL internal format for compressed textures
    uint32_t m_compressedSize = 0;

    uint32_t m_texWidth = 0;
    uint32_t m_texHeight = 0;
    uint32_t m_texPitch = 0;
    uint32_t m_cols = 0;
    uint32_t m_rows = 0;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_pitch = 0;
    ePixelFormat m_format = ePixelFormat::RGB;
    uint32_t m_bitsPerPixel = 0;
    const uint8_t* m_image = nullptr;

    std::vector<sChunk> m_chunks;
    std::vector<sChunk> m_chunksOld;

    std::vector<uint8_t> m_buffer;

    // Pixel readback cache: stores the last-read pixel
    mutable struct sPixelCache
    {
        uint32_t x = UINT32_MAX;
        uint32_t y = UINT32_MAX;
        uint8_t rgba[4] = {};
    } m_pixelCache;
};

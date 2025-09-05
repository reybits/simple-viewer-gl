/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include "types/types.h"
#include "types/vector.h"

#include <vector>

class cQuad;

class cQuadImage final
{
public:
    cQuadImage();
    ~cQuadImage();

    void clear();
    void setBuffer(uint32_t width, uint32_t height, uint32_t pitch, uint32_t format, uint32_t bpp, const uint8_t* image);
    bool upload(uint32_t mipmapTextureSize);

    void stop();
    bool isUploading() const;
    float getProgress() const;

    void useFilter(bool filter);
    void render();

    uint32_t getWidth() const
    {
        return m_width;
    }

    uint32_t getHeight() const
    {
        return m_height;
    }

private:
    void moveToOld();
    void clearOld();
    cQuad* findAndRemoveOld(uint32_t col, uint32_t row);

    struct sChunk
    {
        uint32_t col;
        uint32_t row;
        cQuad* quad;
    };

    bool isInsideViewport(const sChunk& chunk, const Vectorf& pos) const;

private:
    bool m_started = false;
    bool m_filter = false;

    uint32_t m_texWidth = 0;
    uint32_t m_texHeight = 0;
    uint32_t m_texPitch = 0;
    uint32_t m_cols = 0;
    uint32_t m_rows = 0;

    uint32_t m_width = 0;
    uint32_t m_height = 0;
    uint32_t m_pitch = 0;
    uint32_t m_format = 0;
    uint32_t m_bitsPerPixel = 0;
    const uint8_t* m_image = nullptr;

    std::vector<sChunk> m_chunks;
    std::vector<sChunk> m_chunksOld;

    std::vector<uint8_t> m_buffer;
};

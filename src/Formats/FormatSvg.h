/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Format.h"

#include <memory>

namespace lunasvg {
class Document;
}

class cFormatSvg final : public cFormat
{
public:
    explicit cFormatSvg(sCallbacks* callbacks);
    ~cFormatSvg();

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info) override;
    bool LoadSubImageImpl(uint32_t subImage, sChunkData& chunk, sImageInfo& info) override;

    bool rasterize(uint32_t width, uint32_t height, sChunkData& chunk, sImageInfo& info);

    std::unique_ptr<lunasvg::Document> m_document;
    float m_svgWidth = 0.0f;
    float m_svgHeight = 0.0f;
};

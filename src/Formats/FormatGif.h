/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#if defined(GIF_SUPPORT)

#include "Format.h"

#include <gif_lib.h>
#include <memory>
#include <string>

class cFormatGif final : public cFormat
{
public:
    using cFormat::cFormat;

    bool isSupported(cFile& file, Buffer& buffer) const override;

private:
    bool LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info) override;
    bool LoadSubImageImpl(uint32_t current, sChunkData& chunk, sImageInfo& info) override;

    bool load(uint32_t current, sChunkData& chunk, sImageInfo& info);

private:
    std::string m_filename;

    struct PreviousFrame
    {
        uint32_t disposalMode = 0;
        uint32_t left = 0;
        uint32_t top = 0;
        uint32_t width = 0;
        uint32_t height = 0;
    };
    PreviousFrame m_prevFrame;

    struct GifDeleter
    {
        void operator()(GifFileType* b);
    };

    std::unique_ptr<GifFileType, GifDeleter> m_gif;
};

#endif

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
    bool LoadImpl(const char* filename, sBitmapDescription& desc) override;
    bool LoadSubImageImpl(uint32_t current, sBitmapDescription& desc) override;

    bool load(uint32_t current, sBitmapDescription& desc);

private:
    std::string m_filename;

    struct GifDeleter
    {
        void operator()(GifFileType* b);
    };

    std::unique_ptr<GifFileType, GifDeleter> m_gif;
};

#endif

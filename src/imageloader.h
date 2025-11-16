/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include "common/bitmap_description.h"
#include "formats/format.h"

#include <memory>
#include <thread>

class iCallbacks;
struct sConfig;

enum class eImageType
{
#if defined(IMLIB2_SUPPORT)
    COMMON,
#endif
#if defined(OPENEXR_SUPPORT)
    EXR,
#endif
    JPG,
#if defined(JPEG2000_SUPPORT)
    JP2,
#endif
    PSD,
    EPS,
    PNG,
#if defined(GIF_SUPPORT)
    GIF,
#endif
    ICO,
    ICNS,
#if defined(TIFF_SUPPORT)
    TIF,
#endif
    XWD,
    XPM,
    DDS,
    RAW,
    AGE,
    PNM,
    PVR,
    SCR,
    TGA,
    BMP,
    XCF,
    SVG,
#if defined(WEBP_SUPPORT)
    WEBP,
#endif

    NOTAVAILABLE,

    COUNT
};

class cImageLoader final
{
public:
    explicit cImageLoader(const sConfig* config, iCallbacks* callbacks);
    ~cImageLoader();

    void loadImage(const std::string& path);
    void loadSubImage(unsigned subImage);
    bool isLoaded() const;

    enum class Mode
    {
        Image,
        SubImage
    };
    Mode getMode() const
    {
        return m_mode;
    }

    const char* getImageType() const;

    const sBitmapDescription& getDescription() const
    {
        return m_desc;
    }

private:
    cFormat* createLoader(eImageType type) const;
    cFormat* getLoader(eImageType type) const;

    void stop();
    void clear();
    eImageType getType(const char* name) const;
    void load(const char* path);

private:
    const sConfig* m_config;
    iCallbacks* m_callbacks;

    Mode m_mode = Mode::Image;
    std::thread m_loader;
    cFormat* m_image = nullptr;
    mutable std::unique_ptr<cFormat> m_formats[(unsigned)eImageType::COUNT];
    sBitmapDescription m_desc;
};

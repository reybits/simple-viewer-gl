/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "imageloader.h"
#include "common/bitmap_description.h"
#include "common/callbacks.h"
#include "common/file.h"
#include "common/helpers.h"
#include "formats/formatage.h"
#include "formats/formatbmp.h"
#include "formats/formatcommon.h"
#include "formats/formatdds.h"
#include "formats/formateps.h"
#include "formats/formatexr.h"
#include "formats/formatgif.h"
#include "formats/formaticns.h"
#include "formats/formatico.h"
#include "formats/formatjp2k.h"
#include "formats/formatjpeg.h"
#include "formats/formatpng.h"
#include "formats/formatpnm.h"
#include "formats/formatpsd.h"
#include "formats/formatpvr.h"
#include "formats/formatraw.h"
#include "formats/formatscr.h"
#include "formats/formatsvg.h"
#include "formats/formattarga.h"
#include "formats/formattiff.h"
#include "formats/formatwebp.h"
#include "formats/formatxcf.h"
#include "formats/formatxpm.h"
#include "formats/formatxwd.h"
#include "log/Log.h"
#include "network/curl.h"
#include "notavailable.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>

// #define LOADER_NAME

cImageLoader::cImageLoader(const sConfig* config, iCallbacks* callbacks)
    : m_config(config)
    , m_callbacks(callbacks)
{
#if CREATE_ALL_LOADERS_AT_ONCE
    for (uint32_t i = 0; i < (uint32_t)eImageType::COUNT; i++)
    {
        createLoader((eImageType)i);
    }
#endif
}

cImageLoader::~cImageLoader()
{
    stop();
    clear();
}

void cImageLoader::load(const char* path)
{
    if (path != nullptr)
    {
        // auto start = helpers::getTime();
        bool result = false;

        cCurl curl;
        if (curl.isUrl(path))
        {
            if (curl.loadFile(path))
            {
                path = curl.getPath();

                const eImageType type = getType(path);
                m_image = getLoader(type);

                result = m_image->Load(path, m_desc);
            }
        }
        else
        {
            const eImageType type = getType(path);
            m_image = getLoader(type);

            result = m_image->Load(path, m_desc);
        }

        // cLog::Info("Loading time: {} μs.", (helpers::getTime() - start) / 1000);

        if (result == true)
        {
            return;
        }
    }

    m_image = getLoader(eImageType::NOTAVAILABLE);
    m_image->Load(path, m_desc);
}

void cImageLoader::loadImage(const std::string& path)
{
    stop();
    clear();

    m_mode = Mode::Image;
    m_loader = std::thread([this](const std::string& path) {
        m_callbacks->startLoading();
        load(path.c_str());
        if (m_desc.images == 0)
        {
            m_desc.images = 1;
        }
        m_callbacks->endLoading();
    },
                           path);
}

void cImageLoader::loadSubImage(unsigned subImage)
{
    assert(m_image != nullptr);

    stop();

    m_mode = Mode::SubImage;
    m_loader = std::thread([this](unsigned subImage) {
        m_callbacks->startLoading();
        m_image->LoadSubImage(subImage, m_desc);
        m_callbacks->endLoading();
    },
                           subImage);
}

bool cImageLoader::isLoaded() const
{
    if (m_desc.bitmap.empty())
    {
        return false;
    }

    auto notAvailable = getLoader(eImageType::NOTAVAILABLE);
    return m_image != notAvailable;
}

void cImageLoader::stop()
{
    if (m_loader.joinable())
    {
        if (m_image != nullptr)
        {
            m_image->stop();
        }
        m_loader.join();
    }
}

void cImageLoader::clear()
{
    m_desc.reset();
}

const char* cImageLoader::getImageType() const
{
    if (m_image != nullptr)
    {
        return m_image->getFormatName();
    }
    return nullptr;
}

namespace
{
    const eImageType SortedTypes[] = {
        eImageType::JPG,
        eImageType::PNG,
        eImageType::BMP,
#if defined(GIF_SUPPORT)
        eImageType::GIF,
#endif
        eImageType::PSD,
        eImageType::XCF,
        eImageType::ICO,
        eImageType::TGA,
#if defined(TIFF_SUPPORT)
        eImageType::TIF,
#endif
        eImageType::EPS,
        eImageType::DDS,
        eImageType::XWD,
        eImageType::XPM,
        eImageType::PNM,
        eImageType::ICNS,
#if defined(WEBP_SUPPORT)
        eImageType::WEBP,
#endif
#if defined(JPEG2000_SUPPORT)
        eImageType::JP2,
#endif
        eImageType::AGE,
        eImageType::SVG,
        eImageType::RAW, // pretend to remove
        eImageType::PVR,
        eImageType::SCR,

#if defined(OPENEXR_SUPPORT)
        eImageType::EXR,
#endif

#if defined(IMLIB2_SUPPORT)
        eImageType::COMMON, // use it as fallback loader
#endif
    };

#if defined(LOADER_NAME)
    const char* typeToName(eImageType type)
    {
        struct Pair
        {
            const char* name;
            const eImageType type;
        };

        const Pair Names[] = {
#if defined(IMLIB2_SUPPORT)
            { "COMMON", eImageType::COMMON },
#endif
#if defined(OPENEXR_SUPPORT)
            { "EXR", eImageType::EXR },
#endif
            { "JPG", eImageType::JPG },
#if defined(JPEG2000_SUPPORT)
            { "JP2", eImageType::JP2 },
#endif
            { "PSD", eImageType::PSD },
            { "EPS", eImageType::EPS },
            { "PNG", eImageType::PNG },
#if defined(GIF_SUPPORT)
            { "GIF", eImageType::GIF },
#endif
            { "ICO", eImageType::ICO },
            { "ICNS", eImageType::ICNS },
#if defined(TIFF_SUPPORT)
            { "TIF", eImageType::TIF },
#endif
            { "XWD", eImageType::XWD },
            { "XPM", eImageType::XPM },
            { "DDS", eImageType::DDS },
            { "RAW", eImageType::RAW },
            { "AGE", eImageType::AGE },
            { "PNM", eImageType::PNM },
            { "PVR", eImageType::PVR },
            { "SCR", eImageType::SCR },
            { "TGA", eImageType::TGA },
            { "BMP", eImageType::BMP },
            { "XCF", eImageType::XCF },
            { "SVG", eImageType::SVG },
#if defined(WEBP_SUPPORT)
            { "WEBP", eImageType::WEBP },
#endif

            { "NOTAVAILABLE", eImageType::NOTAVAILABLE },
        };

        const auto idx = (size_t)type;
        const auto size = helpers::countof(Names);
        assert(size == (size_t)eImageType::COUNT);
        if (idx < size)
        {
            auto& pair = Names[idx];
            assert(pair.type == type);
            return pair.name;
        }

        return "";
    }
#endif

} // namespace

cFormat* cImageLoader::createLoader(eImageType type) const
{
#if defined(LOADER_NAME)
    cLog::Info("Creating loader {}.", typeToName(type));
#endif

    auto& format = m_formats[(unsigned)type];
    assert(format.get() == nullptr);

    cFormat* loader = nullptr;

    switch (type)
    {
#if defined(IMLIB2_SUPPORT)
    case eImageType::COMMON:
        loader = new cFormatCommon(m_callbacks);
        break;
#endif

#if defined(OPENEXR_SUPPORT)
    case eImageType::EXR:
        loader = new cFormatExr(m_callbacks);
        break;
#endif

#if defined(JPEG2000_SUPPORT)
    case eImageType::JP2:
        loader = new cFormatJp2k(m_callbacks);
        break;
#endif

    case eImageType::JPG:
        loader = new cFormatJpeg(m_callbacks);
        break;

    case eImageType::PSD:
        loader = new cFormatPsd(m_callbacks);
        break;

    case eImageType::EPS:
        loader = new cFormatEps(m_callbacks);
        break;

    case eImageType::PNG:
        loader = new cFormatPng(m_callbacks);
        break;

#if defined(GIF_SUPPORT)
    case eImageType::GIF:
        loader = new cFormatGif(m_callbacks);
        break;
#endif

    case eImageType::ICO:
        loader = new cFormatIco(m_callbacks);
        break;

    case eImageType::ICNS:
        loader = new cFormatIcns(m_callbacks);
        break;

#if defined(TIFF_SUPPORT)
    case eImageType::TIF:
        loader = new cFormatTiff(m_callbacks);
        break;
#endif

    case eImageType::XWD:
        loader = new cFormatXwd(m_callbacks);
        break;

    case eImageType::XPM:
        loader = new cFormatXpm(m_callbacks);
        break;

    case eImageType::DDS:
        loader = new cFormatDds(m_callbacks);
        break;

    case eImageType::RAW:
        loader = new cFormatRaw(m_callbacks);
        break;

    case eImageType::AGE:
        loader = new cFormatAge(m_callbacks);
        break;

    case eImageType::PNM:
        loader = new cFormatPnm(m_callbacks);
        break;

    case eImageType::PVR:
        loader = new cFormatPvr(m_callbacks);
        break;

    case eImageType::SCR:
        loader = new cFormatScr(m_callbacks);
        break;

    case eImageType::TGA:
        loader = new cFormatTarga(m_callbacks);
        break;

    case eImageType::BMP:
        loader = new cFormatBmp(m_callbacks);
        break;

    case eImageType::XCF:
        loader = new cFormatXcf(m_callbacks);
        break;

    case eImageType::SVG:
        loader = new cFormatSvg(m_callbacks);
        break;

#if defined(WEBP_SUPPORT)
    case eImageType::WEBP:
        loader = new cFormatWebP(m_callbacks);
        break;
#endif

    case eImageType::NOTAVAILABLE:
        loader = new cNotAvailable();
        break;

    case eImageType::COUNT: // do nothing
        break;
    }

    if (loader != nullptr)
    {
        loader->setConfig(m_config);
    }

    format.reset(loader);

    return loader;
}

cFormat* cImageLoader::getLoader(eImageType type) const
{
    auto& loader = m_formats[(unsigned)type];
    if (loader.get() == nullptr)
    {
        createLoader(type);
    }

    return loader.get();
}

eImageType cImageLoader::getType(const char* name) const
{
    cFile file;
    if (file.open(name))
    {
        Buffer buffer;
        for (auto type : SortedTypes)
        {
#if defined(LOADER_NAME)
            cLog::Info("Probing loader {}.", typeToName(type));
#endif
            auto loader = getLoader(type);
            if (loader->isSupported(file, buffer))
            {
#if defined(LOADER_NAME)
                cLog::Info("Using loader {}.", typeToName(type));
#endif
                return type;
            }
        }

        cLog::Warning("Loader not available for '{}'.", name);
    }

    return eImageType::NOTAVAILABLE;
}

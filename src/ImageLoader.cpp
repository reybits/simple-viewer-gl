/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "ImageLoader.h"
#include "Common/BitmapDescription.h"
#include "Common/Callbacks.h"
#include "Common/File.h"
#include "Formats/FormatRegistry.h"
#include "Log/Log.h"
#include "Network/Curl.h"
#include "NotAvailable.h"

#include <cassert>
#include <string>

cImageLoader::cImageLoader(const sConfig* config, sCallbacks* callbacks)
    : m_config(config)
    , m_callbacks(callbacks)
{
}

cImageLoader::~cImageLoader()
{
    stop();
    clear();
}

cFormat* cImageLoader::getOrCreateReader(const sFormatEntry& entry)
{
    auto it = m_formatCache.find(entry.name);
    if (it != m_formatCache.end())
    {
        return it->second.get();
    }

    auto reader = entry.factory(m_callbacks);
    reader->setConfig(m_config);
    auto* ptr = reader.get();
    m_formatCache.emplace(entry.name, std::move(reader));
    return ptr;
}

void cImageLoader::load(const char* path)
{
    if (path != nullptr)
    {
        bool result = false;

        cCurl curl;
        if (curl.isUrl(path))
        {
            if (curl.loadFile(path))
            {
                path = curl.getPath();

                cFile file;
                if (file.open(path))
                {
                    Buffer buffer;
                    auto entry = FormatRegistry::detect(file, buffer);
                    if (entry != nullptr)
                    {
                        m_activeReader = getOrCreateReader(*entry);
                        result = m_activeReader->Load(path, m_desc);
                    }
                }
            }
        }
        else
        {
            cFile file;
            if (file.open(path))
            {
                Buffer buffer;
                auto entry = FormatRegistry::detect(file, buffer);
                if (entry != nullptr)
                {
                    m_activeReader = getOrCreateReader(*entry);
                    result = m_activeReader->Load(path, m_desc);
                }
            }
        }

        if (result)
        {
            return;
        }
    }

    // Fallback to "not available"
    static const char* naKey = "n/a";
    auto it = m_formatCache.find(naKey);
    if (it == m_formatCache.end())
    {
        auto na = std::make_unique<cNotAvailable>();
        m_formatCache.emplace(naKey, std::move(na));
        it = m_formatCache.find(naKey);
    }
    m_activeReader = it->second.get();
    m_activeReader->Load(path, m_desc);
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
    assert(m_activeReader != nullptr);

    stop();

    m_mode = Mode::SubImage;
    m_loader = std::thread([this](unsigned subImage) {
        m_callbacks->startLoading();
        m_activeReader->LoadSubImage(subImage, m_desc);
        m_callbacks->endLoading();
    },
                           subImage);
}

bool cImageLoader::isLoaded() const
{
    if (m_desc.bitmap.empty() && m_desc.bitmapSize == 0)
    {
        return false;
    }

    // Check that active reader is not the "not available" fallback
    auto it = m_formatCache.find("n/a");
    if (it != m_formatCache.end() && m_activeReader == it->second.get())
    {
        return false;
    }
    return m_activeReader != nullptr;
}

void cImageLoader::stop()
{
    if (m_loader.joinable())
    {
        if (m_activeReader != nullptr)
        {
            m_activeReader->stop();
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
    return m_desc.formatName;
}

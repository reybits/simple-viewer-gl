/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "ImageLoader.h"
#include "Common/Callbacks.h"
#include "Common/Config.h"
#include "Common/File.h"
#include "Common/Timing.h"
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

bool cImageLoader::loadFromFile(const char* path)
{
    const auto t0 = timing::seconds();

    cFile file;
    if (file.open(path) == false)
    {
        return false;
    }

    Buffer buffer;
    auto entry = FormatRegistry::detect(file, buffer);
    if (entry == nullptr)
    {
        return false;
    }

    m_metrics.fileReadMs = (timing::seconds() - t0) * 1000.0;

    m_activeReader = getOrCreateReader(*entry);
    bool result = m_activeReader->Load(path, m_chunk, m_info);

    if (result)
    {
        m_metrics.decodeMs = m_activeReader->getDecodeMs();
        m_metrics.iccMs = m_activeReader->getIccMs();
    }

    return result;
}

void cImageLoader::load(const char* path)
{
    if (path != nullptr)
    {
        cCurl curl;
        if (curl.isUrl(path))
        {
            if (curl.loadFile(path) && loadFromFile(curl.getPath()))
            {
                return;
            }
        }
        else if (loadFromFile(path))
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
    m_activeReader->Load(path, m_chunk, m_info);
}

void cImageLoader::loadImage(const std::string& path)
{
    stop();
    clear();
    m_metrics.reset();

    m_mode = Mode::Image;
    m_completed.store(false, std::memory_order_relaxed);
    m_loader = std::thread([this](const std::string& path) {
        if (m_config->debug)
        {
            cLog::Debug("=== loading: {} ===", path);
        }
        const auto t0 = timing::seconds();
        m_callbacks->startLoading();
        load(path.c_str());
        if (m_info.images == 0)
        {
            m_info.images = 1;
        }
        m_metrics.bitmapBytes = m_chunk.bitmap.size();
        m_metrics.totalMs = (timing::seconds() - t0) * 1000.0;
        m_completed.store(true, std::memory_order_release);
        m_callbacks->endLoading();
    },
                           path);
}

void cImageLoader::loadSubImage(unsigned subImage)
{
    assert(m_activeReader != nullptr);

    stop();
    m_metrics.reset();

    m_mode = Mode::SubImage;
    m_completed.store(false, std::memory_order_relaxed);
    m_loader = std::thread([this](unsigned subImage) {
        const auto t0 = timing::seconds();
        m_callbacks->startLoading();
        m_activeReader->LoadSubImage(subImage, m_chunk, m_info);
        m_metrics.bitmapBytes = m_chunk.bitmap.size();
        m_metrics.totalMs = (timing::seconds() - t0) * 1000.0;
        m_completed.store(true, std::memory_order_release);
        m_callbacks->endLoading();
    },
                           subImage);
}

bool cImageLoader::isLoaded() const
{
    if (m_chunk.bitmap.empty() && m_chunk.width == 0)
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
        const bool completed = m_completed.load(std::memory_order_acquire);
        if (m_activeReader != nullptr)
        {
            m_activeReader->stop();
        }
        const auto t0 = timing::seconds();
        m_loader.join();
        if (m_config->debug && completed == false)
        {
            const double joinMs = (timing::seconds() - t0) * 1000.0;
            cLog::Debug("  aborted:    join {:.1f} ms", joinMs);
        }
    }
}

void cImageLoader::clear()
{
    m_chunk.reset();
    m_info = {};
}

const char* cImageLoader::getImageType() const
{
    return m_info.formatName;
}

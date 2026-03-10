/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Common/ChunkData.h"
#include "Common/ImageInfo.h"

#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <unordered_map>

class cFormat;
struct sCallbacks;
struct sConfig;
struct sFormatEntry;

class cImageLoader final
{
public:
    struct Metrics
    {
        double fileReadMs = 0.0;
        double decodeMs = 0.0;
        double iccMs = 0.0;
        double totalMs = 0.0;
        size_t bitmapBytes = 0;

        void reset()
        {
            *this = {};
        }
    };

    explicit cImageLoader(const sConfig* config, sCallbacks* callbacks);
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

    const sChunkData& getChunkData() const
    {
        return m_chunk;
    }

    const sImageInfo& getImageInfo() const
    {
        return m_info;
    }

    uint32_t getReadyHeight() const
    {
        return m_chunk.readyHeight.load(std::memory_order_acquire);
    }

    void setConsumedHeight(uint32_t h)
    {
        m_chunk.consumedHeight.store(h, std::memory_order_release);
    }

    const uint8_t* getBitmapData() const
    {
        return m_chunk.bitmap.data();
    }

    bool isBitmapAvailable() const
    {
        return m_chunk.bitmap.empty() == false;
    }

    void releaseBitmap()
    {
        Buffer().swap(m_chunk.bitmap);
    }

    bool wasBitmapModified() const;
    void clearBitmapModified();

    const Metrics& getMetrics() const
    {
        return m_metrics;
    }

    Metrics& metrics()
    {
        return m_metrics;
    }

private:
    cFormat* getOrCreateReader(const sFormatEntry& entry);

    void stop();
    void clear();
    bool loadFromFile(const char* path);
    void load(const char* path);

private:
    const sConfig* m_config;
    sCallbacks* m_callbacks;

    Mode m_mode = Mode::Image;
    std::thread m_loader;
    cFormat* m_activeReader = nullptr;
    std::unordered_map<std::string, std::unique_ptr<cFormat>> m_formatCache;
    sChunkData m_chunk;
    sImageInfo m_info;
    Metrics m_metrics;
    std::atomic<bool> m_completed{ false };
};

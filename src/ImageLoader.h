/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Common/BitmapDescription.h"

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

    const sBitmapDescription& getDescription() const
    {
        return m_desc;
    }

    uint32_t getReadyHeight() const
    {
        return m_desc.readyHeight.load(std::memory_order_acquire);
    }

    const uint8_t* getBitmapData() const
    {
        return m_desc.bitmap.data();
    }

    bool isBitmapAvailable() const
    {
        return m_desc.bitmap.empty() == false;
    }

    void releaseBitmap()
    {
        m_desc.bitmapSize = m_desc.bitmap.size();
        Buffer().swap(m_desc.bitmap);
    }

private:
    cFormat* getOrCreateReader(const sFormatEntry& entry);

    void stop();
    void clear();
    void load(const char* path);

private:
    const sConfig* m_config;
    sCallbacks* m_callbacks;

    Mode m_mode = Mode::Image;
    std::thread m_loader;
    cFormat* m_activeReader = nullptr;
    std::unordered_map<std::string, std::unique_ptr<cFormat>> m_formatCache;
    sBitmapDescription m_desc;
};

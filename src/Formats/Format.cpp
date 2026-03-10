/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "Format.h"
#include "Common/Callbacks.h"
#include "Common/ChunkData.h"
#include "Common/Cms.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
#include "Common/Timing.h"
#include "Log/Log.h"

#include <cassert>

cFormat::cFormat(sCallbacks* callbacks)
    : m_callbacks(callbacks)
{
}

cFormat::~cFormat()
{
}

void cFormat::setConfig(const sConfig* config)
{
    m_config = config;
}

bool cFormat::Load(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    m_stop = false;
    m_chunk = &chunk;
    m_info = &info;
    m_decodeMs = 0.0;
    m_iccMs = 0.0;

    const auto t0 = timing::seconds();
    bool result = LoadImpl(filename, chunk, info);
    m_decodeMs = (timing::seconds() - t0) * 1000.0 - m_iccMs;

    if (result)
    {
        signalImageInfo(); // ensure infobar is updated even if format didn't call it
        chunk.readyHeight.store(chunk.height, std::memory_order_release);
    }
    m_chunk = nullptr;
    m_info = nullptr;
    return result;
}

bool cFormat::LoadSubImage(uint32_t subImage, sChunkData& chunk, sImageInfo& info)
{
    m_stop = false;
    m_chunk = &chunk;
    m_info = &info;
    m_decodeMs = 0.0;
    m_iccMs = 0.0;

    const auto t0 = timing::seconds();
    bool result = LoadSubImageImpl(subImage, chunk, info);
    m_decodeMs = (timing::seconds() - t0) * 1000.0 - m_iccMs;

    if (result)
    {
        signalImageInfo();
        chunk.readyHeight.store(chunk.height, std::memory_order_release);
    }
    m_chunk = nullptr;
    m_info = nullptr;
    return result;
}

void cFormat::dump(const sChunkData& chunk, const sImageInfo& info) const
{
    cLog::Debug("bits per pixel: {}", chunk.bpp);
    cLog::Debug("image bpp:      {}", info.bppImage);
    cLog::Debug("width:          {}", chunk.width);
    cLog::Debug("height:         {}", chunk.height);
    cLog::Debug("pitch:          {}", chunk.pitch);
    cLog::Debug("size:           {}", info.fileSize);
    cLog::Debug("frames count:   {}", info.images);
    cLog::Debug("current frame:  {}", info.current);
    cLog::Debug("animation:      {}", info.isAnimation ? "true" : "false");
    cLog::Debug("frame duration: {}", info.delay);
}

void cFormat::updateProgress(float percent)
{
    if (m_chunk != nullptr)
    {
        m_chunk->readyHeight.store(
            static_cast<uint32_t>(percent * m_chunk->height),
            std::memory_order_release);
    }
    m_callbacks->doProgress(percent);
}

void cFormat::signalImageInfo()
{
    if (m_callbacks != nullptr && m_callbacks->onImageInfo && m_info != nullptr)
    {
        assert(m_chunk->width > 0);
        assert(m_chunk->height > 0);
        assert(m_info->bppImage > 0);
        assert(m_info->formatName != nullptr);
        m_callbacks->onImageInfo(*m_chunk, *m_info);
    }
}

void cFormat::signalBitmapAllocated()
{
    signalImageInfo();
    if (m_callbacks != nullptr && m_chunk != nullptr)
    {
        m_chunk->readyHeight.store(0, std::memory_order_release);
        m_callbacks->onBitmapAllocated(*m_chunk);
    }
}

void cFormat::signalPreviewReady(sPreviewData&& preview)
{
    if (m_callbacks != nullptr && m_callbacks->onPreviewReady)
    {
        m_callbacks->onPreviewReady(std::move(preview));
    }
}

void cFormat::setupBitmap(sChunkData& chunk, sImageInfo& info, uint32_t bpp, ePixelFormat format, const char* formatName)
{
    info.formatName = formatName;
    chunk.allocate(chunk.width, chunk.height, bpp, format);
    signalBitmapAllocated();
}

bool cFormat::openFile(cFile& file, const char* filename, sImageInfo& info) const
{
    if (!file.open(filename))
    {
        return false;
    }
    info.fileSize = file.getSize();
    return true;
}

bool cFormat::readBuffer(cFile& file, Buffer& buffer, uint32_t minSize) const
{
    const auto size = static_cast<uint32_t>(buffer.size());
    if (size < minSize)
    {
        buffer.resize(minSize);
        const uint32_t length = minSize - size;
        if (length != file.read(&buffer[size], length))
        {
            return false;
        }
    }

    return minSize <= buffer.size();
}

bool cFormat::applyIccProfile(sChunkData& chunk, const void* iccProfile, uint32_t iccProfileSize)
{
    const auto t0 = timing::seconds();
    chunk.lutData = cms::generateLut3D(iccProfile, iccProfileSize, chunk.format);
    m_iccMs += (timing::seconds() - t0) * 1000.0;
    if (chunk.lutData.empty() == false)
    {
        chunk.lutSize = cms::LutGridSize;
        return true;
    }
    return false;
}

bool cFormat::applyIccProfile(sChunkData& chunk, const float* chr, const float* wp,
                              const uint16_t* gmr, const uint16_t* gmg, const uint16_t* gmb)
{
    const auto t0 = timing::seconds();
    chunk.lutData = cms::generateLut3D(chr, wp, gmr, gmg, gmb, chunk.format);
    m_iccMs += (timing::seconds() - t0) * 1000.0;
    if (chunk.lutData.empty() == false)
    {
        chunk.lutSize = cms::LutGridSize;
        return true;
    }
    return false;
}

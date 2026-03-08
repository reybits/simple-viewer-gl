/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "Format.h"
#include "Cms/Cms.h"
#include "Common/BitmapDescription.h"
#include "Common/Callbacks.h"
#include "Common/File.h"
#include "Log/Log.h"

#include <cassert>

cFormat::cFormat(sCallbacks* callbacks)
    : m_callbacks(callbacks)
{
    m_cms = std::make_unique<cCMS>();
}

cFormat::~cFormat()
{
}

void cFormat::setConfig(const sConfig* config)
{
    m_config = config;
}

bool cFormat::Load(const char* filename, sBitmapDescription& desc)
{
    m_stop = false;
    m_desc = &desc;
    bool result = LoadImpl(filename, desc);
    if (result)
    {
        desc.readyHeight.store(desc.height, std::memory_order_release);
    }
    m_desc = nullptr;
    return result;
}

bool cFormat::LoadSubImage(uint32_t subImage, sBitmapDescription& desc)
{
    m_stop = false;
    m_desc = &desc;
    bool result = LoadSubImageImpl(subImage, desc);
    if (result)
    {
        desc.readyHeight.store(desc.height, std::memory_order_release);
    }
    m_desc = nullptr;
    return result;
}

void cFormat::dump(sBitmapDescription& desc) const
{
    // cLog::Debug("{}", getFormatName(format));
    cLog::Debug("bits per pixel: {}", desc.bpp);
    cLog::Debug("image bpp:      {}", desc.bppImage);
    cLog::Debug("width:          {}", desc.width);
    cLog::Debug("height:         {}", desc.height);
    cLog::Debug("pitch:          {}", desc.pitch);
    cLog::Debug("size:           {}", desc.size);
    cLog::Debug("frames count:   {}", desc.images);
    cLog::Debug("current frame:  {}", desc.current);
    cLog::Debug("animation:      {}", desc.isAnimation ? "true" : "false");
    cLog::Debug("frame duration: {}", desc.delay);
}

void cFormat::updateProgress(float percent)
{
    if (m_desc != nullptr)
    {
        m_desc->readyHeight.store(
            static_cast<uint32_t>(percent * m_desc->height),
            std::memory_order_release);
    }
    m_callbacks->doProgress(percent);
}

void cFormat::signalBitmapAllocated()
{
    if (m_callbacks != nullptr && m_desc != nullptr)
    {
        m_desc->readyHeight.store(0, std::memory_order_release);
        m_callbacks->onBitmapAllocated(*m_desc);
    }
}

void cFormat::setupBitmap(sBitmapDescription& desc, uint32_t w, uint32_t h,
                          uint32_t bpp, ePixelFormat format, const char* formatName)
{
    desc.formatName = formatName;
    desc.allocate(w, h, bpp, format);
    signalBitmapAllocated();
}

bool cFormat::openFile(cFile& file, const char* filename, sBitmapDescription& desc) const
{
    if (!file.open(filename))
    {
        return false;
    }
    desc.size = file.getSize();
    return true;
}

bool cFormat::readBuffer(cFile& file, Buffer& buffer, uint32_t minSize) const
{
    const uint32_t size = (uint32_t)buffer.size();
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

bool cFormat::applyIccProfile(sBitmapDescription& desc, const void* iccProfile, uint32_t iccProfileSize)
{
    auto type = desc.bpp == 32 ? cCMS::Pixel::Rgba : cCMS::Pixel::Rgb;
    m_cms->createTransform(iccProfile, iccProfileSize, type);
    return applyIccProfile(desc);
}

bool cFormat::applyIccProfile(sBitmapDescription& desc, const float* chr, const float* wp, const uint16_t* gmr, const uint16_t* gmg, const uint16_t* gmb)
{
    auto type = desc.bpp == 32 ? cCMS::Pixel::Rgba : cCMS::Pixel::Rgb;
    m_cms->createTransform(chr, wp, gmr, gmg, gmb, type);
    return applyIccProfile(desc);
}

bool cFormat::applyIccProfile(sBitmapDescription& desc)
{
    bool hasIccProfile = m_cms->hasTransform();

    if (hasIccProfile)
    {
        auto bitmap = desc.bitmap.data();

        for (uint32_t y = 0; y < desc.height; y++)
        {
            m_cms->doTransform(bitmap, bitmap, desc.width);
            bitmap += desc.pitch;

            updateProgress((float)y / desc.height);
        }
    }

    m_cms->destroyTransform();

    return hasIccProfile;
}

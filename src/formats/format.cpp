/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "format.h"
#include "cms/cms.h"
#include "common/bitmap_description.h"
#include "common/callbacks.h"
#include "common/file.h"

#include <cassert>
#include <cstdio>

cFormat::cFormat(iCallbacks* callbacks)
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
    // ::printf("(II) %s\n", getFormatName(format));
    ::printf("(II) bits per pixel: %u\n", desc.bpp);
    ::printf("(II) image bpp:      %u\n", desc.bppImage);
    ::printf("(II) width:          %u\n", desc.width);
    ::printf("(II) height:         %u\n", desc.height);
    ::printf("(II) pitch:          %u\n", desc.pitch);
    ::printf("(II) size:           %ld\n", desc.size);
    ::printf("(II) frames count:   %u\n", desc.images);
    ::printf("(II) current frame:  %u\n", desc.current);
    ::printf("(II) animation:      %s\n", desc.isAnimation ? "true" : "false");
    ::printf("(II) frame duration: %u\n", desc.delay);
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

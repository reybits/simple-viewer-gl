/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include "common/buffer.h"

#include <memory>

class cCMS;
class cFile;
class iCallbacks;
struct sBitmapDescription;
struct sConfig;

class cFormat
{
public:
    virtual ~cFormat();

    void setConfig(const sConfig* config);

    virtual bool isSupported(cFile& file, Buffer& buffer) const = 0;

    bool Load(const char* filename, sBitmapDescription& desc);
    bool LoadSubImage(uint32_t subImage, sBitmapDescription& desc);

    void updateProgress(float percent);
    void signalBitmapAllocated();

    const char* getFormatName() const
    {
        return m_formatName;
    }

    virtual void stop()
    {
        m_stop = true;
    }

    virtual void dump(sBitmapDescription& desc) const;

protected:
    cFormat(iCallbacks* callbacks);
    bool readBuffer(cFile& file, Buffer& buffer, uint32_t minSize) const;
    bool applyIccProfile(sBitmapDescription& desc, const void* iccProfile, uint32_t iccProfileSize);
    bool applyIccProfile(sBitmapDescription& desc, const float* chr, const float* wp, const uint16_t* gmr, const uint16_t* gmg, const uint16_t* gmb);

private:
    bool applyIccProfile(sBitmapDescription& desc);

    virtual bool LoadImpl(const char* filename, sBitmapDescription& desc) = 0;
    virtual bool LoadSubImageImpl(uint32_t /*subImage*/, sBitmapDescription& /*desc*/)
    {
        return false;
    }

private:
    iCallbacks* m_callbacks;
    sBitmapDescription* m_desc = nullptr;
    std::unique_ptr<cCMS> m_cms;

protected:
    const sConfig* m_config = nullptr;
    const char* m_formatName = nullptr;
    bool m_stop = false;
};

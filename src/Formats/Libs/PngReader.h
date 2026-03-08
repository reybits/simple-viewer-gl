/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

class cFile;
class cPngWrapper;
struct sBitmapDescription;

class cPngReader
{
public:
    cPngReader();
    virtual ~cPngReader();

    typedef std::function<void(float percent)> progressCallback;
    typedef std::function<void()> allocatedCallback;

    void setProgressCallback(const progressCallback& callback)
    {
        m_progress = callback;
    }

    void setBitmapAllocatedCallback(const allocatedCallback& callback)
    {
        m_allocated = callback;
    }

    static bool isValid(const uint8_t* data, uint32_t size);

    bool loadPng(sBitmapDescription& desc, const uint8_t* data, uint32_t size);
    bool loadPng(sBitmapDescription& desc, cFile& file);

    using IccProfile = std::vector<uint8_t>;

    const IccProfile& getIccProfile() const
    {
        return m_iccProfile;
    }

private:
    bool doLoadPNG(const cPngWrapper& wrapper, sBitmapDescription& desc);

    void updateProgress(float percent) const
    {
        if (m_progress != nullptr)
        {
            m_progress(percent);
        }
    }

public:
    static const uint32_t HeaderSize = 8;

private:
    progressCallback m_progress = nullptr;
    allocatedCallback m_allocated = nullptr;

    IccProfile m_iccProfile;
};

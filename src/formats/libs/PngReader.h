/**********************************************\
*
*  Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include <cstdint>
#include <functional>
#include <vector>

class cFile;
struct sBitmapDescription;

class cPngReader
{
public:
    cPngReader();
    virtual ~cPngReader();

    typedef std::function<void(float percent)> progressCallback;

    void setProgressCallback(const progressCallback& callback)
    {
        m_progress = callback;
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

    IccProfile m_iccProfile;
};

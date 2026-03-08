/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Types/Types.h"

class cCMS final
{
public:
    cCMS();
    ~cCMS();

    enum class Pixel
    {
        Rgba,
        Rgb,
    };
    void createTransform(const void* iccProfile, uint32_t iccProfileSize, Pixel format) const;
    void createTransform(const float* chr, const float* wp
                         , const uint16_t* gmr
                         , const uint16_t* gmg
                         , const uint16_t* gmb
                         , Pixel format) const;
    void destroyTransform() const;

    void doTransform(void* input, void* output, uint32_t sizeInPixels) const;

    bool hasTransform() const
    {
        return m_transform != nullptr;
    }

private:
    void createTransform(void* inProfile, Pixel format) const;

private:
#if defined(LCMS2_SUPPORT)
    mutable void* m_outProfile = nullptr;
#endif
    mutable void* m_transform = nullptr;
};

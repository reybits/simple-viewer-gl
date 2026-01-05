/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "cms.h"
#include "log/Log.h"

#include <cassert>

#if defined(LCMS2_SUPPORT)
#include <lcms2.h>

void cmsLogErrorHandler(cmsContext /*ContextID*/, cmsUInt32Number ErrorCode, const char* Text)
{
    cLog::Error("LCMS2: ({}) '{}'.", ErrorCode, Text);
}
#endif

cCMS::cCMS()
{
#if defined(LCMS2_SUPPORT)
    m_outProfile = cmsCreate_sRGBProfile();

    cmsSetLogErrorHandler(cmsLogErrorHandler);
#endif
}

cCMS::~cCMS()
{
#if defined(LCMS2_SUPPORT)
    destroyTransform();

    cmsCloseProfile(m_outProfile);
#endif
}

void cCMS::createTransform(const void* iccProfile, uint32_t iccProfileSize, Pixel format) const
{
#if defined(LCMS2_SUPPORT)
    destroyTransform();
    if (iccProfile != nullptr && iccProfileSize != 0)
    {
        auto inProfile = cmsOpenProfileFromMem(iccProfile, iccProfileSize);
        createTransform(inProfile, format);
    }
#else
    (void)iccProfile;
    (void)iccProfileSize;
    (void)format;
#endif
}

void cCMS::createTransform(const float* chr, const float* wp, const uint16_t* gmr, const uint16_t* gmg, const uint16_t* gmb, Pixel format) const
{
#if defined(LCMS2_SUPPORT)
    cmsCIExyYTRIPLE Primaries;
    Primaries.Red.x = chr[0];
    Primaries.Red.y = chr[1];
    Primaries.Green.x = chr[2];
    Primaries.Green.y = chr[3];
    Primaries.Blue.x = chr[4];
    Primaries.Blue.y = chr[5];
    Primaries.Red.Y = Primaries.Green.Y = Primaries.Blue.Y = 1.0;

    cmsCIExyY WhitePoint;
    WhitePoint.x = wp[0];
    WhitePoint.y = wp[1];
    WhitePoint.Y = 1.0;

    cmsToneCurve* Curve[3];
    Curve[0] = cmsBuildTabulatedToneCurve16(nullptr, 256, gmr);
    Curve[1] = cmsBuildTabulatedToneCurve16(nullptr, 256, gmg);
    Curve[2] = cmsBuildTabulatedToneCurve16(nullptr, 256, gmb);

    auto inProfile = cmsCreateRGBProfileTHR(nullptr, &WhitePoint, &Primaries, Curve);

    for (uint32_t i = 0; i < 3; i++)
    {
        cmsFreeToneCurve(Curve[i]);
    }

    createTransform(inProfile, format);
#else
    (void)chr;
    (void)wp;
    (void)gmr;
    (void)gmg;
    (void)gmb;
    (void)format;
#endif
}

void cCMS::createTransform(void* inProfile, Pixel format) const
{
#if defined(LCMS2_SUPPORT)
    if (inProfile != nullptr)
    {
        auto pixel = format == Pixel::Rgb ? TYPE_RGB_8 : TYPE_RGBA_8;
        m_transform = cmsCreateTransform(inProfile, pixel, m_outProfile, pixel, INTENT_PERCEPTUAL, 0);
        cmsCloseProfile(inProfile);
    }
#else
    (void)inProfile;
    (void)format;
#endif
}

void cCMS::destroyTransform() const
{
#if defined(LCMS2_SUPPORT)
    if (m_transform != nullptr)
    {
        cmsDeleteTransform(m_transform);
        m_transform = nullptr;
    }
#endif
}

void cCMS::doTransform(void* input, void* output, uint32_t sizeInPixels) const
{
#if defined(LCMS2_SUPPORT)
    assert(m_transform != nullptr);
    cmsDoTransform(m_transform, input, output, sizeInPixels);
#else
    (void)input;
    (void)output;
    (void)sizeInPixels;
#endif
}

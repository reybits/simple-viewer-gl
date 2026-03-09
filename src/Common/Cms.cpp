/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "Cms.h"
#include "Log/Log.h"

#if defined(LCMS2_SUPPORT)
#include <lcms2.h>

namespace
{
    void CmsLogErrorHandler(cmsContext /*ContextID*/, cmsUInt32Number ErrorCode, const char* Text)
    {
        cLog::Error("LCMS2: ({}) '{}'.", ErrorCode, Text);
    }

    cmsHPROFILE getSrgbProfile()
    {
        static cmsHPROFILE profile = [] {
            cmsSetLogErrorHandler(CmsLogErrorHandler);
            return cmsCreate_sRGBProfile();
        }();
        return profile;
    }

    cmsUInt32Number toLcmsPixelType(ePixelFormat format)
    {
        switch (format)
        {
        case ePixelFormat::RGBA:
            return TYPE_RGBA_8;
        case ePixelFormat::BGRA:
            return TYPE_BGRA_8;
        case ePixelFormat::RGB:
            return TYPE_RGB_8;
        case ePixelFormat::BGR:
            return TYPE_BGR_8;
        case ePixelFormat::Luminance:
            return TYPE_GRAY_8;
        case ePixelFormat::LuminanceAlpha:
            return TYPE_GRAYA_8;
        default:
            return 0;
        }
    }

    bool applyTransform(void* transform, uint8_t* bitmap, uint32_t width,
                        uint32_t height, uint32_t pitch)
    {
        if (transform == nullptr)
        {
            return false;
        }

        for (uint32_t y = 0; y < height; y++)
        {
            cmsDoTransform(transform, bitmap, bitmap, width);
            bitmap += pitch;
        }

        cmsDeleteTransform(transform);
        return true;
    }

    bool transformWithProfile(void* inProfile, uint8_t* bitmap, uint32_t width,
                              uint32_t height, uint32_t pitch, ePixelFormat format)
    {
        if (inProfile == nullptr)
        {
            return false;
        }

        auto pixelType = toLcmsPixelType(format);
        if (pixelType == 0)
        {
            cmsCloseProfile(inProfile);
            return false;
        }

        // Skip transform if profile color space doesn't match bitmap format.
        // E.g. CMYK ICC profile with already-converted RGB bitmap.
        auto profileSpace = cmsGetColorSpace(static_cast<cmsHPROFILE>(inProfile));
        bool compatible = false;
        switch (profileSpace)
        {
        case cmsSigRgbData:
            compatible = (format == ePixelFormat::RGB || format == ePixelFormat::RGBA
                          || format == ePixelFormat::BGR || format == ePixelFormat::BGRA);
            break;
        case cmsSigGrayData:
            compatible = (format == ePixelFormat::Luminance || format == ePixelFormat::LuminanceAlpha);
            break;
        default:
            break;
        }

        if (compatible == false)
        {
            cmsCloseProfile(inProfile);
            return false;
        }

        auto transform = cmsCreateTransform(inProfile, pixelType, getSrgbProfile(), pixelType, INTENT_PERCEPTUAL, 0);
        cmsCloseProfile(inProfile);

        return applyTransform(transform, bitmap, width, height, pitch);
    }

} // namespace

#endif

bool cms::transformBitmap(const void* iccProfile, uint32_t iccProfileSize,
                          uint8_t* bitmap, uint32_t width, uint32_t height,
                          uint32_t pitch, ePixelFormat format)
{
#if defined(LCMS2_SUPPORT)
    if (iccProfile == nullptr || iccProfileSize == 0)
    {
        return false;
    }

    auto inProfile = cmsOpenProfileFromMem(iccProfile, iccProfileSize);
    return transformWithProfile(inProfile, bitmap, width, height, pitch, format);
#else
    (void)iccProfile;
    (void)iccProfileSize;
    (void)bitmap;
    (void)width;
    (void)height;
    (void)pitch;
    (void)format;
    return false;
#endif
}

bool cms::transformBitmap(const float* chr, const float* wp,
                          const uint16_t* gmr, const uint16_t* gmg, const uint16_t* gmb,
                          uint8_t* bitmap, uint32_t width, uint32_t height,
                          uint32_t pitch, ePixelFormat format)
{
#if defined(LCMS2_SUPPORT)
    if (chr == nullptr || wp == nullptr)
    {
        return false;
    }

    cmsCIExyYTRIPLE primaries;
    primaries.Red.x = chr[0];
    primaries.Red.y = chr[1];
    primaries.Green.x = chr[2];
    primaries.Green.y = chr[3];
    primaries.Blue.x = chr[4];
    primaries.Blue.y = chr[5];
    primaries.Red.Y = primaries.Green.Y = primaries.Blue.Y = 1.0;

    cmsCIExyY whitePoint;
    whitePoint.x = wp[0];
    whitePoint.y = wp[1];
    whitePoint.Y = 1.0;

    cmsToneCurve* curve[3];
    curve[0] = cmsBuildTabulatedToneCurve16(nullptr, 256, gmr);
    curve[1] = cmsBuildTabulatedToneCurve16(nullptr, 256, gmg);
    curve[2] = cmsBuildTabulatedToneCurve16(nullptr, 256, gmb);

    auto inProfile = cmsCreateRGBProfileTHR(nullptr, &whitePoint, &primaries, curve);

    for (uint32_t i = 0; i < 3; i++)
    {
        cmsFreeToneCurve(curve[i]);
    }

    return transformWithProfile(inProfile, bitmap, width, height, pitch, format);
#else
    (void)chr;
    (void)wp;
    (void)gmr;
    (void)gmg;
    (void)gmb;
    (void)bitmap;
    (void)width;
    (void)height;
    (void)pitch;
    (void)format;
    return false;
#endif
}

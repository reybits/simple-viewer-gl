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

    enum class ProfileType
    {
        Rgb,
        Gray,
        Cmyk,
        Lab,
    };

    std::vector<uint8_t> sampleLutFromProfile(cmsHPROFILE inProfile, ePixelFormat format)
    {
        if (inProfile == nullptr)
        {
            return {};
        }

        auto profileSpace = cmsGetColorSpace(inProfile);
        auto profileType = ProfileType::Rgb;

        switch (profileSpace)
        {
        case cmsSigRgbData:
            switch (format)
            {
            case ePixelFormat::RGB:
            case ePixelFormat::RGBA:
            case ePixelFormat::BGR:
            case ePixelFormat::BGRA:
            case ePixelFormat::CMYK:
                break;
            default:
                cmsCloseProfile(inProfile);
                return {};
            }
            break;
        case cmsSigGrayData:
            switch (format)
            {
            case ePixelFormat::Luminance:
            case ePixelFormat::LuminanceAlpha:
                profileType = ProfileType::Gray;
                break;
            default:
                cmsCloseProfile(inProfile);
                return {};
            }
            break;
        case cmsSigCmykData:
            if (format != ePixelFormat::CMYK)
            {
                cmsCloseProfile(inProfile);
                return {};
            }
            profileType = ProfileType::Cmyk;
            break;
        case cmsSigLabData:
            switch (format)
            {
            case ePixelFormat::RGB:
            case ePixelFormat::RGBA:
                profileType = ProfileType::Lab;
                break;
            default:
                cmsCloseProfile(inProfile);
                return {};
            }
            break;
        default:
            cmsCloseProfile(inProfile);
            return {};
        }

        // Create transform: input profile → sRGB
        cmsHTRANSFORM transform = nullptr;
        switch (profileType)
        {
        case ProfileType::Gray:
            transform = cmsCreateTransform(inProfile, TYPE_GRAY_8, getSrgbProfile(), TYPE_RGB_8, INTENT_PERCEPTUAL, 0);
            break;
        case ProfileType::Cmyk:
            transform = cmsCreateTransform(inProfile, TYPE_CMYK_8, getSrgbProfile(), TYPE_RGB_8, INTENT_PERCEPTUAL, 0);
            break;
        case ProfileType::Rgb:
            transform = cmsCreateTransform(inProfile, TYPE_RGB_8, getSrgbProfile(), TYPE_RGB_8, INTENT_PERCEPTUAL, 0);
            break;
        case ProfileType::Lab:
            transform = cmsCreateTransform(inProfile, TYPE_Lab_8, getSrgbProfile(), TYPE_RGB_8, INTENT_PERCEPTUAL, 0);
            break;
        }
        cmsCloseProfile(inProfile);

        if (transform == nullptr)
        {
            return {};
        }

        constexpr uint32_t N = cms::LutGridSize;
        std::vector<uint8_t> lut(N * N * N * 3);
        auto out = lut.data();

        for (uint32_t b = 0; b < N; b++)
        {
            for (uint32_t g = 0; g < N; g++)
            {
                for (uint32_t r = 0; r < N; r++)
                {
                    const auto rv = static_cast<uint8_t>(r * 255 / (N - 1));
                    const auto gv = static_cast<uint8_t>(g * 255 / (N - 1));
                    const auto bv = static_cast<uint8_t>(b * 255 / (N - 1));

                    if (profileType == ProfileType::Gray)
                    {
                        cmsDoTransform(transform, &rv, out, 1);
                    }
                    else if (profileType == ProfileType::Cmyk)
                    {
                        // LUT maps raw (C,M,Y) → ICC-correct RGB with K=0.
                        // GPU shader applies K separately: rgb = LUT(C,M,Y) * K.
                        // Grid coordinates (rv,gv,bv) are raw PSD values (0=full ink, 255=no ink).
                        // lcms2 CMYK convention: 0=no ink, 100=full ink (percentage).
                        uint8_t cmyk[4] = {
                            static_cast<uint8_t>(255 - rv),
                            static_cast<uint8_t>(255 - gv),
                            static_cast<uint8_t>(255 - bv),
                            0 // K=0 (no black); K is applied by GPU shader
                        };
                        cmsDoTransform(transform, cmyk, out, 1);
                    }
                    else if (profileType == ProfileType::Lab)
                    {
                        // Grid (r,g,b) = (L,a,b) in ICC Lab8 encoding:
                        // L: 0-255 maps to 0-100, a/b: 0-255 maps to -128..+127
                        uint8_t lab[3] = { rv, gv, bv };
                        cmsDoTransform(transform, lab, out, 1);
                    }
                    else
                    {
                        uint8_t rgb[3] = { rv, gv, bv };
                        cmsDoTransform(transform, rgb, out, 1);
                    }
                    out += 3;
                }
            }
        }

        cmsDeleteTransform(transform);
        return lut;
    }

} // namespace

#endif

std::vector<uint8_t> cms::generateLut3D(const void* iccProfile, uint32_t iccProfileSize, ePixelFormat format)
{
#if defined(LCMS2_SUPPORT)
    if (iccProfile == nullptr || iccProfileSize == 0)
    {
        return {};
    }

    auto inProfile = cmsOpenProfileFromMem(iccProfile, iccProfileSize);
    return sampleLutFromProfile(inProfile, format);
#else
    (void)iccProfile;
    (void)iccProfileSize;
    (void)format;
    return {};
#endif
}

std::vector<uint8_t> cms::generateLut3D(const float* chr, const float* wp,
                                         const uint16_t* gmr, const uint16_t* gmg, const uint16_t* gmb,
                                         ePixelFormat format)
{
#if defined(LCMS2_SUPPORT)
    if (chr == nullptr || wp == nullptr)
    {
        return {};
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

    return sampleLutFromProfile(inProfile, format);
#else
    (void)chr;
    (void)wp;
    (void)gmr;
    (void)gmg;
    (void)gmb;
    (void)format;
    return {};
#endif
}

std::vector<uint8_t> cms::generateLabLut3D()
{
#if defined(LCMS2_SUPPORT)
    auto inProfile = cmsCreateLab4Profile(nullptr); // D50 whitepoint
    return sampleLutFromProfile(inProfile, ePixelFormat::RGB);
#else
    return {};
#endif
}

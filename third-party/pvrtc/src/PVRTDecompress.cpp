/*!
\brief Implementation of the PVRTC Texture Decompression.
\file PVRTDecompress.cpp
\author PowerVR by Imagination, Developer Technology Team
\copyright Copyright (c) Imagination Technologies Limited.
*/

#include <pvrtc/PVRTDecompress.h>

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>

namespace pvr
{
    struct Pixel32
    {
        uint8_t red, green, blue, alpha;
    };

    struct Pixel128S
    {
        int32_t red, green, blue, alpha;
    };

    struct PVRTCWord
    {
        uint32_t u32ModulationData;
        uint32_t u32ColorData;
    };

    struct PVRTCWordIndices
    {
        int P[2], Q[2], R[2], S[2];
    };

    static Pixel32 getColorA(uint32_t u32ColorData)
    {
        Pixel32 color;

        if ((u32ColorData & 0x8000) != 0)
        {
            color.red = static_cast<uint8_t>((u32ColorData & 0x7c00) >> 10);
            color.green = static_cast<uint8_t>((u32ColorData & 0x3e0) >> 5);
            color.blue = static_cast<uint8_t>(u32ColorData & 0x1e) | ((u32ColorData & 0x1e) >> 4);
            color.alpha = static_cast<uint8_t>(0xf);
        }
        else
        {
            color.red = static_cast<uint8_t>((u32ColorData & 0xf00) >> 7) | ((u32ColorData & 0xf00) >> 11);
            color.green = static_cast<uint8_t>((u32ColorData & 0xf0) >> 3) | ((u32ColorData & 0xf0) >> 7);
            color.blue = static_cast<uint8_t>((u32ColorData & 0xe) << 1) | ((u32ColorData & 0xe) >> 2);
            color.alpha = static_cast<uint8_t>((u32ColorData & 0x7000) >> 11);
        }

        return color;
    }

    static Pixel32 getColorB(uint32_t u32ColorData)
    {
        Pixel32 color;

        if (u32ColorData & 0x80000000)
        {
            color.red = static_cast<uint8_t>((u32ColorData & 0x7c000000) >> 26);
            color.green = static_cast<uint8_t>((u32ColorData & 0x3e00000) >> 21);
            color.blue = static_cast<uint8_t>((u32ColorData & 0x1f0000) >> 16);
            color.alpha = static_cast<uint8_t>(0xf);
        }
        else
        {
            color.red = static_cast<uint8_t>(((u32ColorData & 0xf000000) >> 23) | ((u32ColorData & 0xf000000) >> 27));
            color.green = static_cast<uint8_t>(((u32ColorData & 0xf00000) >> 19) | ((u32ColorData & 0xf00000) >> 23));
            color.blue = static_cast<uint8_t>(((u32ColorData & 0xf0000) >> 15) | ((u32ColorData & 0xf0000) >> 19));
            color.alpha = static_cast<uint8_t>((u32ColorData & 0x70000000) >> 27);
        }

        return color;
    }

    static void interpolateColors(Pixel32 P, Pixel32 Q, Pixel32 R, Pixel32 S, Pixel128S* pPixel, uint8_t ui8Bpp)
    {
        uint32_t ui32WordWidth = 4;
        uint32_t ui32WordHeight = 4;
        if (ui8Bpp == 2)
        {
            ui32WordWidth = 8;
        }

        Pixel128S hP = { static_cast<int32_t>(P.red), static_cast<int32_t>(P.green), static_cast<int32_t>(P.blue), static_cast<int32_t>(P.alpha) };
        Pixel128S hQ = { static_cast<int32_t>(Q.red), static_cast<int32_t>(Q.green), static_cast<int32_t>(Q.blue), static_cast<int32_t>(Q.alpha) };
        Pixel128S hR = { static_cast<int32_t>(R.red), static_cast<int32_t>(R.green), static_cast<int32_t>(R.blue), static_cast<int32_t>(R.alpha) };
        Pixel128S hS = { static_cast<int32_t>(S.red), static_cast<int32_t>(S.green), static_cast<int32_t>(S.blue), static_cast<int32_t>(S.alpha) };

        Pixel128S QminusP = { hQ.red - hP.red, hQ.green - hP.green, hQ.blue - hP.blue, hQ.alpha - hP.alpha };
        Pixel128S SminusR = { hS.red - hR.red, hS.green - hR.green, hS.blue - hR.blue, hS.alpha - hR.alpha };

        hP.red *= ui32WordWidth;
        hP.green *= ui32WordWidth;
        hP.blue *= ui32WordWidth;
        hP.alpha *= ui32WordWidth;
        hR.red *= ui32WordWidth;
        hR.green *= ui32WordWidth;
        hR.blue *= ui32WordWidth;
        hR.alpha *= ui32WordWidth;

        if (ui8Bpp == 2)
        {
            for (uint32_t x = 0; x < ui32WordWidth; x++)
            {
                Pixel128S result = { 4 * hP.red, 4 * hP.green, 4 * hP.blue, 4 * hP.alpha };
                Pixel128S dY = { hR.red - hP.red, hR.green - hP.green, hR.blue - hP.blue, hR.alpha - hP.alpha };

                for (uint32_t y = 0; y < ui32WordHeight; y++)
                {
                    pPixel[y * ui32WordWidth + x].red = static_cast<int32_t>((result.red >> 7) + (result.red >> 2));
                    pPixel[y * ui32WordWidth + x].green = static_cast<int32_t>((result.green >> 7) + (result.green >> 2));
                    pPixel[y * ui32WordWidth + x].blue = static_cast<int32_t>((result.blue >> 7) + (result.blue >> 2));
                    pPixel[y * ui32WordWidth + x].alpha = static_cast<int32_t>((result.alpha >> 5) + (result.alpha >> 1));

                    result.red += dY.red;
                    result.green += dY.green;
                    result.blue += dY.blue;
                    result.alpha += dY.alpha;
                }

                hP.red += QminusP.red;
                hP.green += QminusP.green;
                hP.blue += QminusP.blue;
                hP.alpha += QminusP.alpha;

                hR.red += SminusR.red;
                hR.green += SminusR.green;
                hR.blue += SminusR.blue;
                hR.alpha += SminusR.alpha;
            }
        }
        else
        {
            for (uint32_t y = 0; y < ui32WordHeight; y++)
            {
                Pixel128S result = { 4 * hP.red, 4 * hP.green, 4 * hP.blue, 4 * hP.alpha };
                Pixel128S dY = { hR.red - hP.red, hR.green - hP.green, hR.blue - hP.blue, hR.alpha - hP.alpha };

                for (uint32_t x = 0; x < ui32WordWidth; x++)
                {
                    pPixel[y * ui32WordWidth + x].red = static_cast<int32_t>((result.red >> 6) + (result.red >> 1));
                    pPixel[y * ui32WordWidth + x].green = static_cast<int32_t>((result.green >> 6) + (result.green >> 1));
                    pPixel[y * ui32WordWidth + x].blue = static_cast<int32_t>((result.blue >> 6) + (result.blue >> 1));
                    pPixel[y * ui32WordWidth + x].alpha = static_cast<int32_t>((result.alpha >> 4) + (result.alpha));

                    result.red += dY.red;
                    result.green += dY.green;
                    result.blue += dY.blue;
                    result.alpha += dY.alpha;
                }

                hP.red += QminusP.red;
                hP.green += QminusP.green;
                hP.blue += QminusP.blue;
                hP.alpha += QminusP.alpha;

                hR.red += SminusR.red;
                hR.green += SminusR.green;
                hR.blue += SminusR.blue;
                hR.alpha += SminusR.alpha;
            }
        }
    }

    static void unpackModulations(const PVRTCWord& word, int offsetX, int offsetY, int32_t i32ModulationValues[16][8], int32_t i32ModulationModes[16][8], uint8_t ui8Bpp)
    {
        uint32_t WordModMode = word.u32ColorData & 0x1;
        uint32_t ModulationBits = word.u32ModulationData;

        if (ui8Bpp == 2)
        {
            if (WordModMode)
            {
                if (ModulationBits & 0x1)
                {
                    if (ModulationBits & (0x1 << 20))
                    {
                        WordModMode = 3;
                    }
                    else
                    {
                        WordModMode = 2;
                    }

                    if (ModulationBits & (0x1 << 21))
                    {
                        ModulationBits |= (0x1 << 20);
                    }
                    else
                    {
                        ModulationBits &= ~(0x1 << 20);
                    }
                }

                if (ModulationBits & 0x2)
                {
                    ModulationBits |= 0x1;
                }
                else
                {
                    ModulationBits &= ~0x1;
                }

                for (int y = 0; y < 4; y++)
                {
                    for (int x = 0; x < 8; x++)
                    {
                        i32ModulationModes[x + offsetX][y + offsetY] = WordModMode;

                        if (((x ^ y) & 1) == 0)
                        {
                            i32ModulationValues[x + offsetX][y + offsetY] = ModulationBits & 3;
                            ModulationBits >>= 2;
                        }
                    }
                }
            }
            else
            {
                for (int y = 0; y < 4; y++)
                {
                    for (int x = 0; x < 8; x++)
                    {
                        i32ModulationModes[x + offsetX][y + offsetY] = WordModMode;

                        if (ModulationBits & 1)
                        {
                            i32ModulationValues[x + offsetX][y + offsetY] = 0x3;
                        }
                        else
                        {
                            i32ModulationValues[x + offsetX][y + offsetY] = 0x0;
                        }
                        ModulationBits >>= 1;
                    }
                }
            }
        }
        else
        {
            if (WordModMode)
            {
                for (int y = 0; y < 4; y++)
                {
                    for (int x = 0; x < 4; x++)
                    {
                        i32ModulationValues[y + offsetY][x + offsetX] = ModulationBits & 3;
                        if (i32ModulationValues[y + offsetY][x + offsetX] == 1)
                        {
                            i32ModulationValues[y + offsetY][x + offsetX] = 4;
                        }
                        else if (i32ModulationValues[y + offsetY][x + offsetX] == 2)
                        {
                            i32ModulationValues[y + offsetY][x + offsetX] = 14;
                        }
                        else if (i32ModulationValues[y + offsetY][x + offsetX] == 3)
                        {
                            i32ModulationValues[y + offsetY][x + offsetX] = 8;
                        }
                        ModulationBits >>= 2;
                    }
                }
            }
            else
            {
                for (int y = 0; y < 4; y++)
                {
                    for (int x = 0; x < 4; x++)
                    {
                        i32ModulationValues[y + offsetY][x + offsetX] = ModulationBits & 3;
                        i32ModulationValues[y + offsetY][x + offsetX] *= 3;
                        if (i32ModulationValues[y + offsetY][x + offsetX] > 3)
                        {
                            i32ModulationValues[y + offsetY][x + offsetX] -= 1;
                        }
                        ModulationBits >>= 2;
                    }
                }
            }
        }
    }

    static int32_t getModulationValues(int32_t i32ModulationValues[16][8], int32_t i32ModulationModes[16][8], uint32_t xPos, uint32_t yPos, uint8_t ui8Bpp)
    {
        if (ui8Bpp == 2)
        {
            const int RepVals0[4] = { 0, 3, 5, 8 };

            if (i32ModulationModes[xPos][yPos] == 0)
            {
                return RepVals0[i32ModulationValues[xPos][yPos]];
            }
            else
            {
                if (((xPos ^ yPos) & 1) == 0)
                {
                    return RepVals0[i32ModulationValues[xPos][yPos]];
                }
                else if (i32ModulationModes[xPos][yPos] == 1)
                {
                    return (RepVals0[i32ModulationValues[xPos][yPos - 1]] + RepVals0[i32ModulationValues[xPos][yPos + 1]] + RepVals0[i32ModulationValues[xPos - 1][yPos]] + RepVals0[i32ModulationValues[xPos + 1][yPos]] + 2) / 4;
                }
                else if (i32ModulationModes[xPos][yPos] == 2)
                {
                    return (RepVals0[i32ModulationValues[xPos - 1][yPos]] + RepVals0[i32ModulationValues[xPos + 1][yPos]] + 1) / 2;
                }
                else
                {
                    return (RepVals0[i32ModulationValues[xPos][yPos - 1]] + RepVals0[i32ModulationValues[xPos][yPos + 1]] + 1) / 2;
                }
            }
        }
        else if (ui8Bpp == 4)
        {
            return i32ModulationValues[xPos][yPos];
        }

        return 0;
    }

    static void pvrtcGetDecompressedPixels(const PVRTCWord& P, const PVRTCWord& Q, const PVRTCWord& R, const PVRTCWord& S, Pixel32* pColorData, uint8_t ui8Bpp)
    {
        int32_t i32ModulationValues[16][8];
        int32_t i32ModulationModes[16][8];
        Pixel128S upscaledColorA[32];
        Pixel128S upscaledColorB[32];

        uint32_t ui32WordWidth = 4;
        uint32_t ui32WordHeight = 4;
        if (ui8Bpp == 2)
        {
            ui32WordWidth = 8;
        }

        unpackModulations(P, 0, 0, i32ModulationValues, i32ModulationModes, ui8Bpp);
        unpackModulations(Q, ui32WordWidth, 0, i32ModulationValues, i32ModulationModes, ui8Bpp);
        unpackModulations(R, 0, ui32WordHeight, i32ModulationValues, i32ModulationModes, ui8Bpp);
        unpackModulations(S, ui32WordWidth, ui32WordHeight, i32ModulationValues, i32ModulationModes, ui8Bpp);

        interpolateColors(getColorA(P.u32ColorData), getColorA(Q.u32ColorData), getColorA(R.u32ColorData), getColorA(S.u32ColorData), upscaledColorA, ui8Bpp);
        interpolateColors(getColorB(P.u32ColorData), getColorB(Q.u32ColorData), getColorB(R.u32ColorData), getColorB(S.u32ColorData), upscaledColorB, ui8Bpp);

        for (uint32_t y = 0; y < ui32WordHeight; y++)
        {
            for (uint32_t x = 0; x < ui32WordWidth; x++)
            {
                int32_t mod = getModulationValues(i32ModulationValues, i32ModulationModes, x + ui32WordWidth / 2, y + ui32WordHeight / 2, ui8Bpp);
                bool punchthroughAlpha = false;
                if (mod > 10)
                {
                    punchthroughAlpha = true;
                    mod -= 10;
                }

                Pixel128S result;
                result.red = (upscaledColorA[y * ui32WordWidth + x].red * (8 - mod) + upscaledColorB[y * ui32WordWidth + x].red * mod) / 8;
                result.green = (upscaledColorA[y * ui32WordWidth + x].green * (8 - mod) + upscaledColorB[y * ui32WordWidth + x].green * mod) / 8;
                result.blue = (upscaledColorA[y * ui32WordWidth + x].blue * (8 - mod) + upscaledColorB[y * ui32WordWidth + x].blue * mod) / 8;
                if (punchthroughAlpha)
                {
                    result.alpha = 0;
                }
                else
                {
                    result.alpha = (upscaledColorA[y * ui32WordWidth + x].alpha * (8 - mod) + upscaledColorB[y * ui32WordWidth + x].alpha * mod) / 8;
                }

                if (ui8Bpp == 2)
                {
                    pColorData[y * ui32WordWidth + x].red = static_cast<uint8_t>(result.red);
                    pColorData[y * ui32WordWidth + x].green = static_cast<uint8_t>(result.green);
                    pColorData[y * ui32WordWidth + x].blue = static_cast<uint8_t>(result.blue);
                    pColorData[y * ui32WordWidth + x].alpha = static_cast<uint8_t>(result.alpha);
                }
                else if (ui8Bpp == 4)
                {
                    pColorData[y + x * ui32WordHeight].red = static_cast<uint8_t>(result.red);
                    pColorData[y + x * ui32WordHeight].green = static_cast<uint8_t>(result.green);
                    pColorData[y + x * ui32WordHeight].blue = static_cast<uint8_t>(result.blue);
                    pColorData[y + x * ui32WordHeight].alpha = static_cast<uint8_t>(result.alpha);
                }
            }
        }
    }

    static uint32_t wrapWordIndex(uint32_t numWords, int word)
    {
        return ((word + numWords) % numWords);
    }

    static bool isPowerOf2(uint32_t input)
    {
        if (!input)
        {
            return 0;
        }

        uint32_t minus1 = input - 1;
        return ((input | minus1) == (input ^ minus1));
    }

    static uint32_t TwiddleUV(uint32_t XSize, uint32_t YSize, uint32_t XPos, uint32_t YPos)
    {
        uint32_t MinDimension = XSize;
        uint32_t MaxValue = YPos;
        uint32_t Twiddled = 0;
        uint32_t SrcBitPos = 1;
        uint32_t DstBitPos = 1;
        int ShiftCount = 0;

        assert(YPos < YSize);
        assert(XPos < XSize);
        assert(isPowerOf2(YSize));
        assert(isPowerOf2(XSize));

        if (YSize < XSize)
        {
            MinDimension = YSize;
            MaxValue = XPos;
        }

        while (SrcBitPos < MinDimension)
        {
            if (YPos & SrcBitPos)
            {
                Twiddled |= DstBitPos;
            }

            if (XPos & SrcBitPos)
            {
                Twiddled |= (DstBitPos << 1);
            }

            SrcBitPos <<= 1;
            DstBitPos <<= 2;
            ShiftCount += 1;
        }

        MaxValue >>= ShiftCount;
        Twiddled |= (MaxValue << (2 * ShiftCount));

        return Twiddled;
    }

    static void mapDecompressedData(Pixel32* pOutput, int width, const Pixel32* pWord, const PVRTCWordIndices& words, uint8_t ui8Bpp)
    {
        uint32_t ui32WordWidth = 4;
        uint32_t ui32WordHeight = 4;
        if (ui8Bpp == 2)
        {
            ui32WordWidth = 8;
        }

        for (uint32_t y = 0; y < ui32WordHeight / 2; y++)
        {
            for (uint32_t x = 0; x < ui32WordWidth / 2; x++)
            {
                pOutput[(((words.P[1] * ui32WordHeight) + y + ui32WordHeight / 2) * width + words.P[0] * ui32WordWidth + x + ui32WordWidth / 2)] = pWord[y * ui32WordWidth + x];
                pOutput[(((words.Q[1] * ui32WordHeight) + y + ui32WordHeight / 2) * width + words.Q[0] * ui32WordWidth + x)] = pWord[y * ui32WordWidth + x + ui32WordWidth / 2];
                pOutput[(((words.R[1] * ui32WordHeight) + y) * width + words.R[0] * ui32WordWidth + x + ui32WordWidth / 2)] = pWord[(y + ui32WordHeight / 2) * ui32WordWidth + x];
                pOutput[(((words.S[1] * ui32WordHeight) + y) * width + words.S[0] * ui32WordWidth + x)] = pWord[(y + ui32WordHeight / 2) * ui32WordWidth + x + ui32WordWidth / 2];
            }
        }
    }

    static int pvrtcDecompress(uint8_t* pCompressedData, Pixel32* pDecompressedData, uint32_t ui32Width, uint32_t ui32Height, uint8_t ui8Bpp)
    {
        uint32_t ui32WordWidth = 4;
        uint32_t ui32WordHeight = 4;
        if (ui8Bpp == 2)
        {
            ui32WordWidth = 8;
        }

        uint32_t* pWordMembers = (uint32_t*)pCompressedData;
        Pixel32* pOutData = pDecompressedData;

        int i32NumXWords = static_cast<int>(ui32Width / ui32WordWidth);
        int i32NumYWords = static_cast<int>(ui32Height / ui32WordHeight);

        PVRTCWordIndices indices;
        Pixel32* pPixels;
        pPixels = static_cast<Pixel32*>(malloc(ui32WordWidth * ui32WordHeight * sizeof(Pixel32)));

        for (int wordY = -1; wordY < i32NumYWords - 1; wordY++)
        {
            for (int wordX = -1; wordX < i32NumXWords - 1; wordX++)
            {
                indices.P[0] = wrapWordIndex(i32NumXWords, wordX);
                indices.P[1] = wrapWordIndex(i32NumYWords, wordY);
                indices.Q[0] = wrapWordIndex(i32NumXWords, wordX + 1);
                indices.Q[1] = wrapWordIndex(i32NumYWords, wordY);
                indices.R[0] = wrapWordIndex(i32NumXWords, wordX);
                indices.R[1] = wrapWordIndex(i32NumYWords, wordY + 1);
                indices.S[0] = wrapWordIndex(i32NumXWords, wordX + 1);
                indices.S[1] = wrapWordIndex(i32NumYWords, wordY + 1);

                uint32_t WordOffsets[4] = {
                    TwiddleUV(i32NumXWords, i32NumYWords, indices.P[0], indices.P[1]) * 2,
                    TwiddleUV(i32NumXWords, i32NumYWords, indices.Q[0], indices.Q[1]) * 2,
                    TwiddleUV(i32NumXWords, i32NumYWords, indices.R[0], indices.R[1]) * 2,
                    TwiddleUV(i32NumXWords, i32NumYWords, indices.S[0], indices.S[1]) * 2,
                };

                PVRTCWord P, Q, R, S;
                P.u32ColorData = static_cast<uint32_t>(pWordMembers[WordOffsets[0] + 1]);
                P.u32ModulationData = static_cast<uint32_t>(pWordMembers[WordOffsets[0]]);
                Q.u32ColorData = static_cast<uint32_t>(pWordMembers[WordOffsets[1] + 1]);
                Q.u32ModulationData = static_cast<uint32_t>(pWordMembers[WordOffsets[1]]);
                R.u32ColorData = static_cast<uint32_t>(pWordMembers[WordOffsets[2] + 1]);
                R.u32ModulationData = static_cast<uint32_t>(pWordMembers[WordOffsets[2]]);
                S.u32ColorData = static_cast<uint32_t>(pWordMembers[WordOffsets[3] + 1]);
                S.u32ModulationData = static_cast<uint32_t>(pWordMembers[WordOffsets[3]]);

                pvrtcGetDecompressedPixels(P, Q, R, S, pPixels, ui8Bpp);
                mapDecompressedData(pOutData, ui32Width, pPixels, indices, ui8Bpp);
            }
        }

        free(pPixels);
        return ui32Width * ui32Height / static_cast<uint32_t>((ui32WordWidth / 2));
    }

    uint32_t PVRTDecompressPVRTC(const void* pCompressedData, uint32_t Do2bitMode, uint32_t XDim, uint32_t YDim, uint8_t* pResultImage)
    {
        Pixel32* pDecompressedData = (Pixel32*)pResultImage;

        uint32_t XTrueDim = std::max(XDim, ((Do2bitMode == 1u) ? 16u : 8u));
        uint32_t YTrueDim = std::max(YDim, 8u);

        if (XTrueDim != XDim || YTrueDim != YDim)
        {
            pDecompressedData = static_cast<Pixel32*>(malloc(XTrueDim * YTrueDim * sizeof(Pixel32)));
        }

        int retval = pvrtcDecompress((uint8_t*)pCompressedData, pDecompressedData, XTrueDim, YTrueDim, (Do2bitMode == 1 ? 2 : 4));

        if (XTrueDim != XDim || YTrueDim != YDim)
        {
            for (uint32_t x = 0; x < XDim; ++x)
            {
                for (uint32_t y = 0; y < YDim; ++y)
                {
                    ((Pixel32*)pResultImage)[x + y * XDim] = pDecompressedData[x + y * XTrueDim];
                }
            }

            free(pDecompressedData);
        }
        return retval;
    }

} // namespace pvr

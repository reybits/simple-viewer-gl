/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatDds.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
#include "Log/Log.h"

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace
{
    // microsoft
    enum DXGI_FORMAT
    {
        DXGI_FORMAT_UNKNOWN = 0,
        DXGI_FORMAT_R32G32B32A32_TYPELESS = 1,
        DXGI_FORMAT_R32G32B32A32_FLOAT = 2,
        DXGI_FORMAT_R32G32B32A32_UINT = 3,
        DXGI_FORMAT_R32G32B32A32_SINT = 4,
        DXGI_FORMAT_R32G32B32_TYPELESS = 5,
        DXGI_FORMAT_R32G32B32_FLOAT = 6,
        DXGI_FORMAT_R32G32B32_UINT = 7,
        DXGI_FORMAT_R32G32B32_SINT = 8,
        DXGI_FORMAT_R16G16B16A16_TYPELESS = 9,
        DXGI_FORMAT_R16G16B16A16_FLOAT = 10,
        DXGI_FORMAT_R16G16B16A16_UNORM = 11,
        DXGI_FORMAT_R16G16B16A16_UINT = 12,
        DXGI_FORMAT_R16G16B16A16_SNORM = 13,
        DXGI_FORMAT_R16G16B16A16_SINT = 14,
        DXGI_FORMAT_R32G32_TYPELESS = 15,
        DXGI_FORMAT_R32G32_FLOAT = 16,
        DXGI_FORMAT_R32G32_UINT = 17,
        DXGI_FORMAT_R32G32_SINT = 18,
        DXGI_FORMAT_R32G8X24_TYPELESS = 19,
        DXGI_FORMAT_D32_FLOAT_S8X24_UINT = 20,
        DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS = 21,
        DXGI_FORMAT_X32_TYPELESS_G8X24_UINT = 22,
        DXGI_FORMAT_R10G10B10A2_TYPELESS = 23,
        DXGI_FORMAT_R10G10B10A2_UNORM = 24,
        DXGI_FORMAT_R10G10B10A2_UINT = 25,
        DXGI_FORMAT_R11G11B10_FLOAT = 26,
        DXGI_FORMAT_R8G8B8A8_TYPELESS = 27,
        DXGI_FORMAT_R8G8B8A8_UNORM = 28,
        DXGI_FORMAT_R8G8B8A8_UNORM_SRGB = 29,
        DXGI_FORMAT_R8G8B8A8_UINT = 30,
        DXGI_FORMAT_R8G8B8A8_SNORM = 31,
        DXGI_FORMAT_R8G8B8A8_SINT = 32,
        DXGI_FORMAT_R16G16_TYPELESS = 33,
        DXGI_FORMAT_R16G16_FLOAT = 34,
        DXGI_FORMAT_R16G16_UNORM = 35,
        DXGI_FORMAT_R16G16_UINT = 36,
        DXGI_FORMAT_R16G16_SNORM = 37,
        DXGI_FORMAT_R16G16_SINT = 38,
        DXGI_FORMAT_R32_TYPELESS = 39,
        DXGI_FORMAT_D32_FLOAT = 40,
        DXGI_FORMAT_R32_FLOAT = 41,
        DXGI_FORMAT_R32_UINT = 42,
        DXGI_FORMAT_R32_SINT = 43,
        DXGI_FORMAT_R24G8_TYPELESS = 44,
        DXGI_FORMAT_D24_UNORM_S8_UINT = 45,
        DXGI_FORMAT_R24_UNORM_X8_TYPELESS = 46,
        DXGI_FORMAT_X24_TYPELESS_G8_UINT = 47,
        DXGI_FORMAT_R8G8_TYPELESS = 48,
        DXGI_FORMAT_R8G8_UNORM = 49,
        DXGI_FORMAT_R8G8_UINT = 50,
        DXGI_FORMAT_R8G8_SNORM = 51,
        DXGI_FORMAT_R8G8_SINT = 52,
        DXGI_FORMAT_R16_TYPELESS = 53,
        DXGI_FORMAT_R16_FLOAT = 54,
        DXGI_FORMAT_D16_UNORM = 55,
        DXGI_FORMAT_R16_UNORM = 56,
        DXGI_FORMAT_R16_UINT = 57,
        DXGI_FORMAT_R16_SNORM = 58,
        DXGI_FORMAT_R16_SINT = 59,
        DXGI_FORMAT_R8_TYPELESS = 60,
        DXGI_FORMAT_R8_UNORM = 61,
        DXGI_FORMAT_R8_UINT = 62,
        DXGI_FORMAT_R8_SNORM = 63,
        DXGI_FORMAT_R8_SINT = 64,
        DXGI_FORMAT_A8_UNORM = 65,
        DXGI_FORMAT_R1_UNORM = 66,
        DXGI_FORMAT_R9G9B9E5_SHAREDEXP = 67,
        DXGI_FORMAT_R8G8_B8G8_UNORM = 68,
        DXGI_FORMAT_G8R8_G8B8_UNORM = 69,
        DXGI_FORMAT_BC1_TYPELESS = 70,
        DXGI_FORMAT_BC1_UNORM = 71,
        DXGI_FORMAT_BC1_UNORM_SRGB = 72,
        DXGI_FORMAT_BC2_TYPELESS = 73,
        DXGI_FORMAT_BC2_UNORM = 74,
        DXGI_FORMAT_BC2_UNORM_SRGB = 75,
        DXGI_FORMAT_BC3_TYPELESS = 76,
        DXGI_FORMAT_BC3_UNORM = 77,
        DXGI_FORMAT_BC3_UNORM_SRGB = 78,
        DXGI_FORMAT_BC4_TYPELESS = 79,
        DXGI_FORMAT_BC4_UNORM = 80,
        DXGI_FORMAT_BC4_SNORM = 81,
        DXGI_FORMAT_BC5_TYPELESS = 82,
        DXGI_FORMAT_BC5_UNORM = 83,
        DXGI_FORMAT_BC5_SNORM = 84,
        DXGI_FORMAT_B5G6R5_UNORM = 85,
        DXGI_FORMAT_B5G5R5A1_UNORM = 86,
        DXGI_FORMAT_B8G8R8A8_UNORM = 87,
        DXGI_FORMAT_B8G8R8X8_UNORM = 88,
        DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM = 89,
        DXGI_FORMAT_B8G8R8A8_TYPELESS = 90,
        DXGI_FORMAT_B8G8R8A8_UNORM_SRGB = 91,
        DXGI_FORMAT_B8G8R8X8_TYPELESS = 92,
        DXGI_FORMAT_B8G8R8X8_UNORM_SRGB = 93,
        DXGI_FORMAT_BC6H_TYPELESS = 94,
        DXGI_FORMAT_BC6H_UF16 = 95,
        DXGI_FORMAT_BC6H_SF16 = 96,
        DXGI_FORMAT_BC7_TYPELESS = 97,
        DXGI_FORMAT_BC7_UNORM = 98,
        DXGI_FORMAT_BC7_UNORM_SRGB = 99,
        DXGI_FORMAT_AYUV = 100,
        DXGI_FORMAT_Y410 = 101,
        DXGI_FORMAT_Y416 = 102,
        DXGI_FORMAT_NV12 = 103,
        DXGI_FORMAT_P010 = 104,
        DXGI_FORMAT_P016 = 105,
        DXGI_FORMAT_420_OPAQUE = 106,
        DXGI_FORMAT_YUY2 = 107,
        DXGI_FORMAT_Y210 = 108,
        DXGI_FORMAT_Y216 = 109,
        DXGI_FORMAT_NV11 = 110,
        DXGI_FORMAT_AI44 = 111,
        DXGI_FORMAT_IA44 = 112,
        DXGI_FORMAT_P8 = 113,
        DXGI_FORMAT_A8P8 = 114,
        DXGI_FORMAT_B4G4R4A4_UNORM = 115,
        DXGI_FORMAT_FORCE_UINT = 0xffffffffUL
    };

    enum D3D10_RESOURCE_DIMENSION
    {
        D3D10_RESOURCE_DIMENSION_UNKNOWN = 0,
        D3D10_RESOURCE_DIMENSION_BUFFER = 1,
        D3D10_RESOURCE_DIMENSION_TEXTURE1D = 2,
        D3D10_RESOURCE_DIMENSION_TEXTURE2D = 3,
        D3D10_RESOURCE_DIMENSION_TEXTURE3D = 4
    };

    // microsoft
    struct DDS_PIXELFORMAT
    {
        uint32_t dwSize;
        uint32_t dwFlags;
        uint32_t dwFourCC;
        uint32_t dwRGBBitCount;
        uint32_t dwRBitMask;
        uint32_t dwGBitMask;
        uint32_t dwBBitMask;
        uint32_t dwABitMask;
    };

    struct DDS_HEADER_DXT10
    {
        DXGI_FORMAT dxgiFormat;
        D3D10_RESOURCE_DIMENSION resourceDimension;
        uint32_t miscFlag;
        uint32_t arraySize;
        uint32_t miscFlags2;
    };

    // microsoft
    struct DDS_HEADER
    {
        uint32_t dwMagic;
        uint32_t dwSize;
        uint32_t dwFlags;
        uint32_t dwHeight;
        uint32_t dwWidth;
        uint32_t dwPitchOrLinearSize;
        uint32_t dwDepth;
        uint32_t dwMipMapCount;
        uint32_t dwReserved1[11];
        DDS_PIXELFORMAT ddspf;
        uint32_t dwCaps;
        uint32_t dwCaps2;
        uint32_t dwCaps3;
        uint32_t dwCaps4;
        uint32_t dwReserved2;
    };

    // struct dds_colorkey
    //{
    // uint32_t dwColorSpaceLowValue;
    // uint32_t dwColorSpaceHighValue;
    // };

    // struct dds_header
    //{
    // uint32_t magic;
    // uint32_t dwSize;
    // uint32_t dwFlags;
    // uint32_t dwHeight;
    // uint32_t dwWidth;
    // long lPitch;
    // uint32_t dwDepth;
    // uint32_t dwMipMapCount;
    // uint32_t dwAlphaBitDepth;
    // uint32_t dwReserved;
    // void* lpSurface;
    // dds_colorkey ddckCKDestOverlay;
    // dds_colorkey ddckCKDestBlt;
    // dds_colorkey ddckCKSrcOverlay;
    // dds_colorkey ddckCKSrcBlt;
    // uint32_t dwPFSize;
    // uint32_t dwPFFlags;
    // uint32_t dwFourCC;
    // uint32_t dwRGBBitCount;
    // uint32_t dwRBitMask;
    // uint32_t dwGBitMask;
    // uint32_t dwBBitMask;
    // uint32_t dwRGBAlphaBitMask;
    // uint32_t dwCaps;
    // uint32_t dwCaps2;
    // uint32_t dwCaps3;
    // uint32_t dwVolumeDepth;
    // uint32_t dwTextureStage;
    // };

    struct dds_color
    {
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    enum DDPF_FLAGS
    {
        DDPF_ALPHAPIXELS = 0x1,
        DDPF_ALPHA = 0x2,
        DDPF_FOURCC = 0x4,
        DDPF_RGB = 0x40,
        DDPF_YUV = 0x200,
        DDPF_LUMINANCE = 0x20000
    };

    enum DDSD_FLAGS
    {
        DDSD_CAPS = 0x1,
        DDSD_HEIGHT = 0x2,
        DDSD_WIDTH = 0x4,
        DDSD_PITCH = 0x8,
        DDSD_PIXELFORMAT = 0x1000,
        DDSD_MIPMAPCOUNT = 0x20000,
        DDSD_LINEARSIZE = 0x80000,
        DDSD_DEPTH = 0x800000
    };

    typedef enum DDS_FORMAT
    {
        DDS_ERROR = -1,
        DDS_RGB,
        DDS_RGBA,
        DDS_DXT1,
        DDS_DXT2,
        DDS_DXT3,
        DDS_DXT4,
        DDS_DXT5,
        DDS_DXT10
    } DDS_FORMAT;

    const char* formatToStirng(DDS_FORMAT fmt)
    {
        static const char* formats[] = {
            "dds/rgb", "dds/rgba", "dds/dxt1", "dds/dxt2", "dds/dxt3", "dds/dxt4", "dds/dxt5", "dds/dxt10"
        };

        return fmt != DDS_ERROR ? formats[fmt] : "dds/Unknown format";
    }

    bool isValidFormat(const DDS_HEADER& header, uint32_t fileSize)
    {
        if (sizeof(header) >= fileSize)
        {
            return false;
        }

        const uint32_t DDS_MAGIC = ('D' | 'D' << 8 | 'S' << 16 | ' ' << 24);
        return header.dwMagic == DDS_MAGIC && header.dwSize == 124
            && (header.dwFlags & DDSD_CAPS)
            && (header.dwFlags & DDSD_HEIGHT)
            && (header.dwFlags & DDSD_WIDTH)
            && (header.dwFlags & DDSD_PIXELFORMAT);
    }

} // namespace

bool cFormatDds::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, sizeof(DDS_HEADER)))
    {
        return false;
    }

    auto header = reinterpret_cast<const DDS_HEADER*>(buffer.data());
    return isValidFormat(*header, file.getSize());
}

bool cFormatDds::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (!openFile(file, filename, info))
    {
        return false;
    }

    DDS_HEADER header;
    if (sizeof(header) != file.read(&header, sizeof(header)))
    {
        cLog::Error("Invalid DDS header size.");
        return false;
    }

    if (!isValidFormat(header, info.fileSize))
    {
        cLog::Error("Invalid DDS header.");
        return false;
    }

    chunk.width = header.dwWidth;
    chunk.height = header.dwHeight;

    DDS_FORMAT format = DDS_ERROR;
    DDS_HEADER_DXT10 header10;
    if (header.ddspf.dwFlags == DDPF_FOURCC)
    {
        if (header.ddspf.dwFourCC == ('D' | 'X' << 8 | '1' << 16 | '0' << 24))
        {
            if (sizeof(header10) != file.read(&header10, sizeof(header10)))
            {
                cLog::Error("Can't load DDS file '{}': invalid DX10 header size.", filename);
                return false;
            }
            format = DDS_DXT10;
        }
        else
        {
            switch (header.ddspf.dwFourCC)
            {
            case ('D' | 'X' << 8 | 'T' << 16 | '1' << 24):
                format = DDS_DXT1;
                break;
            case ('D' | 'X' << 8 | 'T' << 16 | '2' << 24):
                format = DDS_DXT2;
                break;
            case ('D' | 'X' << 8 | 'T' << 16 | '3' << 24):
                format = DDS_DXT3;
                break;
            case ('D' | 'X' << 8 | 'T' << 16 | '4' << 24):
                format = DDS_DXT4;
                break;
            case ('D' | 'X' << 8 | 'T' << 16 | '5' << 24):
                format = DDS_DXT5;
                break;
            }
        }
    }
    else if (header.ddspf.dwFlags & DDPF_RGB && header.ddspf.dwFlags & DDPF_ALPHAPIXELS)
    {
        format = DDS_RGBA;
    }
    else
    {
        format = DDS_RGB;
    }

    if (format == DDS_ERROR)
    {
        cLog::Error("Can't load DDS file '{}': unknown format {:#x} RGB {}.", filename, header.ddspf.dwFlags, header.ddspf.dwRGBBitCount);
        return false;
    }

    const uint32_t data_size = info.fileSize - file.getOffset();
    std::vector<uint8_t> buffer(data_size);
    uint8_t* src = buffer.data();
    if (data_size != file.read(src, data_size))
    {
        cLog::Error("Can't load DDS file '{}': invalid data size.", filename);
        return false;
    }

    info.formatName = formatToStirng(format);

    if (format == DDS_RGB)
    {
        info.bppImage = 24;
        chunk.allocate(chunk.width, chunk.height, 24, ePixelFormat::RGB);
        uint8_t* dest = chunk.bitmap.data();

        for (uint32_t y = 0; y < chunk.height; y++)
        {
            for (uint32_t x = 0; x < chunk.width; x++)
            {
                *dest++ = *src++;
                *dest++ = *src++;
                *dest++ = *src++;
            }
        }
    }
    else if (format == DDS_RGBA)
    {
        info.bppImage = 32;
        chunk.allocate(chunk.width, chunk.height, 32, ePixelFormat::RGBA);
        uint8_t* dest = chunk.bitmap.data();

        for (uint32_t y = 0; y < chunk.height; y++)
        {
            for (uint32_t x = 0; x < chunk.width; x++)
            {
                *dest++ = *src++;
                *dest++ = *src++;
                *dest++ = *src++;
                *dest++ = *src++;
            }
        }
    }
    else
    {
        chunk.format = ePixelFormat::RGBA;
        chunk.bpp = 32;
        info.bppImage = 32;
        chunk.pitch = chunk.width * 4;
        const uint32_t h = (uint32_t)::ceilf(chunk.height / 4.0f) * 4;
        const uint32_t size = chunk.pitch * h;
        chunk.bitmap.resize(size);

        for (uint32_t y = 0; y < chunk.height; y += 4)
        {
            for (uint32_t x = 0; x < chunk.width; x += 4)
            {
                uint64_t alpha = 0;
                uint32_t a0 = 0;
                uint32_t a1 = 0;
                dds_color color[4];
                if (format == DDS_DXT3)
                {
                    alpha = *(uint64_t*)src;
                    src += 8;
                }
                else if (format == DDS_DXT5)
                {
                    alpha = (*(uint64_t*)src) >> 16;
                    a0 = src[0];
                    a1 = src[1];
                    src += 8;
                }
                uint32_t c0 = *(uint16_t*)(src + 0);
                uint32_t c1 = *(uint16_t*)(src + 2);
                src += 4;
                color[0].r = ((c0 >> 11) & 0x1f) << 3;
                color[0].g = ((c0 >> 5) & 0x3f) << 2;
                color[0].b = ((c0 >> 0) & 0x1f) << 3;
                color[1].r = ((c1 >> 11) & 0x1f) << 3;
                color[1].g = ((c1 >> 5) & 0x3f) << 2;
                color[1].b = ((c1 >> 0) & 0x1f) << 3;
                if (c0 > c1)
                {
                    color[2].r = (color[0].r * 2 + color[1].r) / 3;
                    color[2].g = (color[0].g * 2 + color[1].g) / 3;
                    color[2].b = (color[0].b * 2 + color[1].b) / 3;
                    color[3].r = (color[0].r + color[1].r * 2) / 3;
                    color[3].g = (color[0].g + color[1].g * 2) / 3;
                    color[3].b = (color[0].b + color[1].b * 2) / 3;
                }
                else
                {
                    color[2].r = (color[0].r + color[1].r) / 2;
                    color[2].g = (color[0].g + color[1].g) / 2;
                    color[2].b = (color[0].b + color[1].b) / 2;
                    color[3].r = 0;
                    color[3].g = 0;
                    color[3].b = 0;
                }

                for (uint32_t i = 0; i < 4; i++)
                {
                    uint32_t index = *src++;
                    uint8_t* dest = chunk.bitmap.data() + (chunk.width * (y + i) + x) * 4;
                    for (uint32_t j = 0; j < 4; j++)
                    {
                        *dest++ = color[index & 0x03].r;
                        *dest++ = color[index & 0x03].g;
                        *dest++ = color[index & 0x03].b;
                        if (format == DDS_DXT1)
                        {
                            *dest++ = ((index & 0x03) == 3 && c0 <= c1) ? 0 : 255;
                        }
                        else if (format == DDS_DXT3)
                        {
                            *dest++ = (alpha & 0x0f) << 4;
                            alpha >>= 4;
                        }
                        else if (format == DDS_DXT5)
                        {
                            uint32_t a = alpha & 0x07;
                            if (a == 0)
                            {
                                *dest++ = a0;
                            }
                            else if (a == 1)
                            {
                                *dest++ = a1;
                            }
                            else if (a0 > a1)
                            {
                                *dest++ = ((8 - a) * a0 + (a - 1) * a1) / 7;
                            }
                            else if (a > 5)
                            {
                                *dest++ = (a == 6) ? 0 : 255;
                            }
                            else
                            {
                                *dest++ = ((6 - a) * a0 + (a - 1) * a1) / 5;
                            }
                            alpha >>= 3;
                        }
                        else
                        {
                            *dest++ = 255;
                        }

                        index >>= 2;
                    }
                }
            }
        }
    }

    return true;
}

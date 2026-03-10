/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatRaw.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/ImageInfo.h"
#include "Libs/Rle.h"
#include "Log/Log.h"

#include <cstring>

namespace
{
    const char Id[] = { 'R', 'A', 'W', 'I' };

    enum eFormat
    {
        FORMAT_UNKNOWN,
        FORMAT_RGB,
        FORMAT_RGBA,
        FORMAT_RGB_RLE,
        FORMAT_RGBA_RLE,
        FORMAT_RGB_RLE4,
        FORMAT_RGBA_RLE4
    };

    struct sHeader
    {
        unsigned id;
        unsigned w;
        unsigned h;
        unsigned format;
        unsigned data_size;
    };

    bool isValidFormat(const sHeader& header, unsigned file_size)
    {
        if (header.data_size + sizeof(sHeader) == file_size)
        {
            const char* id = (const char*)&header.id;
            return (id[0] == Id[0] && id[1] == Id[1] && id[2] == Id[2] && id[3] == Id[3]);
        }
        return false;
    }

    // bool cFormatRaw::isRawFormat(const char* name)
    // {
    //     cFile file;
    //     if (!file.open(name))
    //     {
    //         return false;
    //     }
    //
    //     sHeader header;
    //     if (sizeof(header) != file.read(&header, sizeof(header)))
    //     {
    //         return false;
    //     }
    //
    //     return isValidFormat(header, file.getSize());
    // }

} // namespace

bool cFormatRaw::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, sizeof(sHeader)))
    {
        return false;
    }

    auto header = reinterpret_cast<const sHeader*>(buffer.data());
    return isValidFormat(*header, file.getSize());
}

bool cFormatRaw::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (!openFile(file, filename, info))
    {
        return false;
    }

    sHeader header;
    if (sizeof(header) != file.read(&header, sizeof(header)))
    {
        cLog::Error("Invalid RAW format.");
        return false;
    }

    if (!isValidFormat(header, info.fileSize))
    {
        return false;
    }

    bool rle = true;
    unsigned bytespp = 0;
    switch (header.format)
    {
    case FORMAT_RGB:
        bytespp = 3;
        rle = false;
        break;
    case FORMAT_RGBA:
        bytespp = 4;
        rle = false;
        break;
    case FORMAT_RGB_RLE:
    case FORMAT_RGB_RLE4:
        bytespp = 3;
        break;
    case FORMAT_RGBA_RLE:
    case FORMAT_RGBA_RLE4:
        bytespp = 4;
        break;
    default:
        cLog::Error("Unknown RAW format.");
        return false;
    }
    info.bppImage = bytespp * 8;
    chunk.width = header.w;
    chunk.height = header.h;
    chunk.allocate(chunk.width, chunk.height, bytespp * 8, bytespp == 3 ? ePixelFormat::RGB : ePixelFormat::RGBA);

    if (rle)
    {
        std::vector<unsigned char> rle(header.data_size);
        if (header.data_size != file.read(&rle[0], header.data_size))
        {
            return false;
        }

        updateProgress(0.5f);

        cRLE decoder;
        unsigned decoded = 0;
        if (header.format == FORMAT_RGB_RLE4 || header.format == FORMAT_RGBA_RLE4)
        {
            decoded = decoder.decodeBy4((unsigned*)&rle[0], rle.size() / 4, (unsigned*)&chunk.bitmap[0], chunk.bitmap.size() / 4);
            info.formatName = "raw/rle32";
        }
        else
        {
            decoded = decoder.decode(&rle[0], rle.size(), &chunk.bitmap[0], chunk.bitmap.size());
            info.formatName = "raw/rle";
        }
        if (!decoded)
        {
            cLog::Error("Can't decode RLE data.");
            return false;
        }

        updateProgress(1.0f);
    }
    else
    {
        for (unsigned y = 0; y < chunk.height; y++)
        {
            if (chunk.pitch != file.read(&chunk.bitmap[y * chunk.pitch], chunk.pitch))
            {
                return false;
            }
            updateProgress((float)y / chunk.height);
        }
    }

    info.formatName = "raw";

    return true;
}

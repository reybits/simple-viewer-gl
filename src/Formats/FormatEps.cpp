/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatEps.h"
#include "Common/BitmapDescription.h"
#include "Common/File.h"
#include "Common/Helpers.h"

#include <cstdio>
#include <cstring>

namespace
{
    bool getContent(const char* data, size_t size, const char* name, std::string& out)
    {
        auto begin = helpers::memfind(data, size, name);
        if (begin != nullptr)
        {
            begin += ::strlen(name) + 1;
            auto end = helpers::memfind(begin, size, name);
            if (end != nullptr)
            {
                out.assign(begin, end - begin - 2);
                helpers::replaceAll(out, "&#xA;", "");

                return true;
            }
        }

        return false;
    }

    void addExifTag(const char* data, size_t size, const char* name, sBitmapDescription::ExifList& exifList)
    {
        std::string out;
        if (getContent(data, size, name, out))
        {
            exifList.push_back({ name, out });
        }
    }
} // namespace

bool cFormatEps::isSupported(cFile& file, Buffer& buffer) const
{
    for (uint32_t bufferSize = 256; bufferSize < 40 * 1024; bufferSize <<= 1)
    {
        if (!readBuffer(file, buffer, std::min<uint32_t>(file.getSize(), bufferSize)))
        {
            return false;
        }

        auto data = (const char*)buffer.data();
        auto size = (uint32_t)buffer.size();

        const auto eps = helpers::memfind(data, size, "!PS-Adobe");
        if (eps != nullptr)
        {
            return true;
        }

        const auto ai = helpers::memfind(data, size, "Adobe XMP Core");
        if (ai != nullptr)
        {
            return true;
        }

        if (file.getSize() == size)
        {
            return false;
        }
    }

    return false;
}

bool cFormatEps::LoadImpl(const char* filename, sBitmapDescription& desc)
{
    cFile file;
    if (!openFile(file, filename, desc))
    {
        return false;
    }

    const uint32_t size = file.getSize();

    Buffer buffer;
    buffer.resize(size);
    auto data = buffer.data();

    if (file.read(data, size) != file.getSize())
    {
        ::printf("(EE) Error loading EPS/AI.\n");
        return false;
    }

    std::string base64;
    if (getContent((const char*)data, size, "xmpGImg:image", base64))
    {
        Buffer decoded;
        if (helpers::base64decode(base64.data(), base64.size(), decoded))
        {
            bool result = decodeJpeg(decoded.data(), decoded.size(), desc);
            if (result)
            {
                desc.formatName = "eps";

                auto& exifList = desc.exifList;
                addExifTag((const char*)data, size, "xmp:CreatorTool", exifList);
                addExifTag((const char*)data, size, "xmp:CreateDate", exifList);
                addExifTag((const char*)data, size, "xmp:ModifyDate", exifList);
                addExifTag((const char*)data, size, "xmp:MetadataDate", exifList);

                return true;
            }
        }
    }
    else
    {
        ::printf("(EE) Cant get xmpGImg:image\n");
    }

    return false;
}

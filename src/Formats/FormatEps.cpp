/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatEps.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/Helpers.h"
#include "Common/ImageInfo.h"
#include "Log/Log.h"

#include <cstring>

namespace
{
    bool GetContent(const char* data, size_t size, const char* name, std::string& out)
    {
        auto begin = helpers::memfind(data, size, name);
        if (begin != nullptr)
        {
            begin += std::strlen(name) + 1;
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

    using eCategory = sImageInfo::ExifCategory;

    void AddExifTag(const char* data, size_t size, const char* name, eCategory category, sImageInfo::ExifList& exifList)
    {
        std::string out;
        if (GetContent(data, size, name, out))
        {
            exifList.push_back({ category, name, out });
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

        auto data = reinterpret_cast<const char*>(buffer.data());
        auto size = static_cast<uint32_t>(buffer.size());

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

bool cFormatEps::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (openFile(file, filename, info) == false)
    {
        return false;
    }

    const auto size = file.getSize();

    Buffer buffer;
    buffer.resize(size);
    if (file.read(buffer.data(), size) != file.getSize())
    {
        cLog::Error("Can't load EPS/AI file.");
        return false;
    }

    auto data = reinterpret_cast<const char*>(buffer.data());

    std::string base64;
    if (GetContent(data, size, "xmpGImg:image", base64))
    {
        Buffer decoded;
        if (helpers::base64decode(base64.data(), base64.size(), decoded))
        {
            auto progressCb = [this](float p) { updateProgress(p); };
            auto allocatedCb = [this]() { signalBitmapAllocated(); };
            auto imageInfoCb = [this]() { signalImageInfo(); };
            auto result = m_decoder.decodeJpeg(decoded.data(), static_cast<uint32_t>(decoded.size()), chunk, info, progressCb, allocatedCb, imageInfoCb, nullptr, m_stop);
            if (result.success)
            {
                // ICC is applied per-scanline inside decodeJpeg()
                info.formatName = result.iccProfile.empty() ? "eps" : "eps/icc";

                auto& exifList = info.exifList;
                AddExifTag(data, size, "xmp:CreatorTool", eCategory::Software, exifList);
                AddExifTag(data, size, "xmp:CreateDate", eCategory::Date, exifList);
                AddExifTag(data, size, "xmp:ModifyDate", eCategory::Date, exifList);
                AddExifTag(data, size, "xmp:MetadataDate", eCategory::Date, exifList);

                return true;
            }
        }
    }
    else
    {
        cLog::Error("Can't get xmpGImg:image.");
    }

    return false;
}

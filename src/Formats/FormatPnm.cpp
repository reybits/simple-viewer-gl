/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FormatPnm.h"
#include "Common/ChunkData.h"
#include "Common/File.h"
#include "Common/Helpers.h"
#include "Common/ImageInfo.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
    bool isEndLine(char ch)
    {
        const char delims[] = "\n\0";

        for (uint32_t i = 0; i < std::size(delims); i++)
        {
            if (delims[i] == ch)
            {
                return true;
            }
        }
        return false;
    }

    typedef std::vector<char> Line;

    bool getline(Line& line, cFile& file)
    {
        line.clear();

        auto remain = file.getSize() - file.getOffset();
        if (remain == 0)
        {
            // ::printf("- eof\n");
            return false;
        }

        const uint32_t bufferSize = 20;
        uint32_t bufferOffset = 0;

        while (remain != 0)
        {
            auto offset = file.getOffset();

            const auto size = std::min<uint32_t>(remain, bufferSize);
            line.resize(line.size() + size);

            auto buffer = line.data() + bufferOffset;
            if (file.read(buffer, size) != size)
            {
                line.push_back(0);
                // ::printf("- error read file\n");
                return true;
            }

            for (uint32_t i = 0; i < size; i++)
            {
                auto ch = buffer[i];
                if (isEndLine(ch))
                {
                    offset += i + (ch == 0 ? 0 : 1);
                    line.resize(bufferOffset + i + 1);
                    line[line.size() - 1] = 0;
                    file.seek(offset, SEEK_SET);
                    // ::printf("- stop char detected, seek to: %u\n", (uint32_t)offset);
                    return true;
                }
            }

            remain -= size;
            bufferOffset += size;
        }

        return true;
    }

    const char* TokenSep = "\n\t ";

    bool readAscii1(cFile& file, sChunkData& chunk, sImageInfo& info)
    {
        info.bppImage = 1;
        chunk.allocate(chunk.width, chunk.height, 8, ePixelFormat::Luminance);

        Line line;

        uint32_t x = 0;
        uint32_t y = 0;

        auto out = chunk.bitmap.data();
        while (getline(line, file))
        {
            for (auto word = ::strtok(line.data(), TokenSep); word != nullptr; word = ::strtok(nullptr, TokenSep))
            {
                const auto val = (uint32_t)::atoi(word) != 0 ? 0 : 255;
                if (x == chunk.width)
                {
                    y++;
                    x = 0;
                }
                const size_t idx = y * chunk.pitch + x++;
                out[idx] = val;
            }
        }

        return true;
    }

    bool readRaw1(cFile& file, sChunkData& chunk, sImageInfo& info)
    {
        info.bppImage = 1;
        chunk.allocate(chunk.width, chunk.height, 8, ePixelFormat::Luminance);
        const uint32_t width = (uint32_t)::ceilf(chunk.width / 8.0f) * 8;
        std::vector<uint8_t> buffer(width / 8);

        for (uint32_t row = 0; row < chunk.height; row++)
        {
            if (buffer.size() != file.read(buffer.data(), buffer.size()))
            {
                return false;
            }

            auto out = chunk.bitmap.data() + row * chunk.pitch;
            size_t idx = 0;
            for (uint32_t i = 0; i < buffer.size(); i++)
            {
                const auto byte = buffer[i];
                for (uint32_t b = 0; b < 8; b++)
                {
                    const uint32_t bit = 0x80 >> b;
                    const uint8_t val = (byte & bit) != 0 ? 0 : 255;
                    out[idx++] = val;
                }
            }
        }

        return true;
    }

    bool readAscii8(cFile& file, sChunkData& chunk, sImageInfo& info, uint32_t maxValue)
    {
        info.bppImage = 8;
        chunk.allocate(chunk.width, chunk.height, 8, ePixelFormat::Luminance);

        Line line;

        const float norm = 255.0f / maxValue;

        uint32_t count = 0;
        uint32_t x = 0;
        uint32_t y = 0;

        auto out = chunk.bitmap.data();
        while (getline(line, file))
        {
            for (auto word = ::strtok(line.data(), TokenSep); word != nullptr; word = ::strtok(nullptr, TokenSep))
            {
                count++;
                const auto val = (uint32_t)(::atoi(word) * norm);
                if (x == chunk.width)
                {
                    y++;
                    x = 0;
                }
                const size_t idx = y * chunk.pitch + x++;
                out[idx] = val;
            }
        }

        return count == chunk.width * chunk.height;
    }

    bool readRaw8(cFile& file, sChunkData& chunk, sImageInfo& info, uint32_t maxValue)
    {
        info.bppImage = 8;
        chunk.allocate(chunk.width, chunk.height, 8, ePixelFormat::Luminance);
        std::vector<uint8_t> buffer(chunk.width);

        const float norm = 255.0f / maxValue;

        for (uint32_t row = 0; row < chunk.height; row++)
        {
            file.read(buffer.data(), buffer.size());

            auto out = chunk.bitmap.data() + row * chunk.pitch;
            for (uint32_t i = 0; i < chunk.width; i++)
            {
                const auto val = (uint8_t)(buffer[i] * norm);
                out[i] = val;
            }
        }

        return true;
    }

    bool readAscii24(cFile& file, sChunkData& chunk, sImageInfo& info, uint32_t maxValue)
    {
        chunk.bpp = info.bppImage = 24;
        chunk.pitch = helpers::calculatePitch(chunk.width, chunk.bpp);
        chunk.resizeBitmap(chunk.pitch, chunk.height);

        Line line;

        const float norm = 255.0f / maxValue;

        uint32_t count = 0;
        uint32_t x = 0;
        uint32_t y = 0;

        auto out = chunk.bitmap.data();
        while (getline(line, file))
        {
            for (auto word = ::strtok(line.data(), TokenSep); word != nullptr; word = ::strtok(nullptr, TokenSep))
            {
                count++;
                const auto val = (uint32_t)(::atoi(word) * norm);
                if (x == chunk.width * 3)
                {
                    y++;
                    x = 0;
                }
                const size_t idx = y * chunk.pitch + x++;
                out[idx] = val;
            }
        }

        return count == chunk.width * chunk.height * 3;
    }

    bool readRaw24(cFile& file, sChunkData& chunk, sImageInfo& info, uint32_t maxValue)
    {
        chunk.bpp = info.bppImage = 24;
        chunk.pitch = helpers::calculatePitch(chunk.width, chunk.bpp);
        chunk.resizeBitmap(chunk.pitch, chunk.height);

        auto out = chunk.bitmap.data();
        const float norm = 255.0f / maxValue;

        std::vector<uint8_t> buffer(chunk.width * 3);
        for (uint32_t y = 0; y < chunk.height; y++)
        {
            if (buffer.size() != file.read(buffer.data(), buffer.size()))
            {
                return false;
            }

            if (maxValue < 255)
            {
                for (size_t i = 0, size = buffer.size(); i < size; i++)
                {
                    buffer[i] *= norm;
                }
            }

            ::memcpy(out + chunk.pitch * y, buffer.data(), buffer.size());
        }

        return true;
    }
} // namespace

bool cFormatPnm::isSupported(cFile& file, Buffer& buffer) const
{
    if (!readBuffer(file, buffer, 2))
    {
        return false;
    }

    const auto h = reinterpret_cast<const char*>(buffer.data());
    return h[0] == 'P' && h[1] >= '1' && h[1] <= '6' && file.getSize() >= 8;
}

bool cFormatPnm::LoadImpl(const char* filename, sChunkData& chunk, sImageInfo& info)
{
    cFile file;
    if (!openFile(file, filename, info))
    {
        return false;
    }

    bool result = false;
    Line line;

    uint32_t format = 0;
    uint32_t maxValue = 0;

    enum class Token
    {
        Format,
        Width,
        Height,
        MaxValue,
        Data,
    };
    Token token = Token::Format;

    while (token != Token::Data)
    {
        if (getline(line, file))
        {
            if (line.size() == 0 || line[0] == '#')
            {
                continue;
            }

            auto begin = line.data();
            const char* word = nullptr;
            while ((word = ::strsep(&begin, TokenSep)) != nullptr && token != Token::Data)
            {
                const auto wordLen = ::strlen(word);
                if (wordLen == 0)
                {
                    continue;
                }

                // ::printf("token: '%s'\n", word);

                switch (token)
                {
                case Token::Format:
                    if (wordLen >= 2 && word[0] == 'P')
                    {
                        token = Token::Width;
                        format = word[1] - '0';
                    }
                    break;

                case Token::Width:
                    token = Token::Height;
                    chunk.width = (uint32_t)::atoi(word);
                    break;

                case Token::Height:
                    token = (format != 4 && format != 1) ? Token::MaxValue : Token::Data;
                    chunk.height = (uint32_t)::atoi(word);
                    break;

                case Token::MaxValue:
                    token = Token::Data;
                    maxValue = (uint32_t)::atoi(word);
                    break;

                case Token::Data: // do nothing
                    break;
                }
            }
        }
    }

    maxValue = std::max<uint32_t>(maxValue, 1);

    switch (format)
    {
    case 1: // 1-ascii
        info.formatName = "pnm/1-acii";
        result = readAscii1(file, chunk, info);
        break;

    case 4: // 1-raw
        info.formatName = "pnm/1-raw";
        result = readRaw1(file, chunk, info);
        break;

    case 2: // 8-ascii
        info.formatName = "pnm/8-acii";
        result = readAscii8(file, chunk, info, maxValue);
        break;

    case 5: // 8-raw
        info.formatName = "pnm/8-raw";
        result = readRaw8(file, chunk, info, maxValue);
        break;

    case 3: // 24-ascii
        info.formatName = "pnm/24-acii";
        result = readAscii24(file, chunk, info, maxValue);
        break;

    case 6: // 24-raw
        info.formatName = "pnm/24-raw";
        result = readRaw24(file, chunk, info, maxValue);
        break;
    }

    return result;
}

/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "CachedReader.h"
#include "Common/File.h"

#include <cstring>

cCachedReader::cCachedReader(cFile& file, uint32_t bufferSize)
    : m_file(file)
    , m_buffer(bufferSize)
{
}

bool cCachedReader::read(uint8_t* buffer, uint32_t count)
{
    if (count <= m_remain)
    {
        ::memcpy(buffer, m_buffer.data() + m_offset, count);
        m_offset += count;
        m_remain -= count;
        return true;
    }
    else
    {
        do
        {
            uint32_t toCopy = std::min<uint32_t>(count, m_remain);

            ::memcpy(buffer, m_buffer.data() + m_offset, toCopy);

            buffer += toCopy;
            count -= toCopy;

            m_offset += toCopy;
            m_remain -= toCopy;

            if (m_remain == 0)
            {
                uint32_t remain = m_file.getSize() - m_file.getOffset();
                uint32_t toRead = std::min<uint32_t>(remain, m_buffer.size());

                m_offset = 0;
                m_remain = toRead;

                if (m_file.read(m_buffer.data(), toRead) != toRead)
                {
                    return false;
                }
            }
        } while (count > 0);
    }

    return true;
}

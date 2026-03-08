/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "FileZlib.h"

#include <cstring>
#include <zlib.h>

cFileZlib::cFileZlib(cFile* file)
    : cFileInterface()
    , m_file(file)
    , m_zipStream(nullptr)
{
    m_zipStream = new z_stream_s();
    z_streamp st = (z_streamp)m_zipStream;

    ::memset(st, 0, sizeof(z_stream_s));
    st->data_type = Z_BINARY;

    inflateInit(st);

    constexpr uint32_t BufferSize = 4 * 1024;
    m_buffer.resize(BufferSize);
}

cFileZlib::~cFileZlib()
{
    inflateEnd(m_zipStream);
    delete m_zipStream;
}

long cFileZlib::getOffset() const
{
    return 0;
}

int cFileZlib::seek(long /*offset*/, int /*whence*/)
{
    return 0;
}

uint32_t cFileZlib::read(void* ptr, uint32_t size)
{
    z_streamp st = m_zipStream;

    st->next_out = static_cast<Bytef*>(ptr);
    st->avail_out = size;

    uint32_t readed = 0;
    do
    {
        if (st->avail_in == 0)
        {
            st->avail_in = m_file->read(m_buffer.data(), m_buffer.size());
            st->next_in = m_buffer.data();
        }

        st->total_out = 0;
        inflate(st, Z_FULL_FLUSH);

        readed += st->total_out;
    } while (st->total_out > 0);

    return readed;
}

long cFileZlib::getSize() const
{
    return 0;
}

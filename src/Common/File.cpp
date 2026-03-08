/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "File.h"
#include "Log/Log.h"

#include <cstdio>

cFile::~cFile()
{
    close();
}

bool cFile::open(const char* path, const char* mode)
{
    FILE* file = fopen(path, mode);
    if (file != nullptr)
    {
        m_file = file;

        (void)fseek(file, 0, SEEK_END);
        m_size = ftell(file);
        (void)fseek(file, 0, SEEK_SET);

        m_path = path;

        return true;
    }

    cLog::Error("Can't open '{}'.", path);
    return false;
}

void cFile::close()
{
    if (m_file != nullptr)
    {
        fclose((FILE*)m_file);
        m_path = nullptr;
        m_file = nullptr;
    }
}

int cFile::seek(long offset, int whence)
{
    return fseek((FILE*)m_file, offset, whence);
}

uint32_t cFile::read(void* ptr, uint32_t size)
{
    if (m_file != nullptr)
    {
        return fread(ptr, 1, size, (FILE*)m_file);
    }

    return 0;
}

long cFile::getOffset() const
{
    if (m_file != nullptr)
    {
        return ftell((FILE*)m_file);
    }

    return 0;
}

/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Types/Types.h"

class cFileInterface
{
public:
    virtual ~cFileInterface() { }

    virtual long getOffset() const = 0;
    virtual int seek(long offset, int whence) = 0;
    virtual uint32_t read(void* ptr, uint32_t size) = 0;
    virtual long getSize() const = 0;
};



class cFile : public cFileInterface
{
public:
    virtual ~cFile();

    bool open(const char* path, const char* mode = "rb");
    void close();

    void* getHandle() const { return m_file; }

    virtual long getOffset() const override;
    virtual int seek(long offset, int whence) override;
    virtual uint32_t read(void* ptr, uint32_t size) override;
    virtual long getSize() const override { return m_size; }

protected:
    const char* m_path = nullptr;
    void* m_file = nullptr;
    long m_size = 0;
};

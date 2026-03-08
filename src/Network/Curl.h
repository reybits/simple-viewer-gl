/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include <vector>

class cCurl final
{
public:
    cCurl();
    ~cCurl();

    bool isUrl(const char* path) const;

    bool loadFile(const char* path);

    const char* getPath() const
    {
        return m_path.data();
    }

private:
    void deleteFile();

private:
    std::vector<char> m_path;
};

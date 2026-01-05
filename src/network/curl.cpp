/**********************************************\
*
*  Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "curl.h"
#include "log/Log.h"

#include <cstring>
#if defined(CURL_SUPPORT)
#include <cstdlib>
#include <curl/curl.h>
#include <unistd.h>
#endif

cCurl::cCurl()
{
}

cCurl::~cCurl()
{
    deleteFile();
}

bool cCurl::isUrl(const char* path) const
{
    return ::strncmp(path, "http://", 7) == 0
        || ::strncmp(path, "https://", 8) == 0
        || ::strncmp(path, "ftp://", 6) == 0
        || ::strncmp(path, "file://", 7) == 0;
}

bool cCurl::loadFile(const char* path)
{
#if defined(CURL_SUPPORT)
    const char* url = path;

    char tmpDir[] = "/tmp/sviewgl.XXXXXX";

    const char* n = ::strrchr(path, '/');
    if (n != nullptr)
    {
        path = n + 1;
    }

    if (::mkdtemp(tmpDir) == nullptr)
    {
        cLog::Error("Can't create temp file '{}'.", path);
        return false;
    }

    m_path.resize(::strlen(tmpDir) + ::strlen(path) + 2);
    ::snprintf(m_path.data(), m_path.size(), "%s/%s", tmpDir, path);

    path = m_path.data();

    cLog::Info("Using temp file '{}'.", path);

    auto file = ::fopen(path, "wb");
    if (file == nullptr)
    {
        cLog::Error("Can't write to file '{}'.", path);
        return false;
    }

    auto curl = ::curl_easy_init();
    auto result = curl != nullptr;
    if (result)
    {
#if defined(DEBUG)
        ::curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
#endif

        ::curl_easy_setopt(curl, CURLOPT_URL, url);
        ::curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);

        auto res = ::curl_easy_perform(curl);
        (void)res;

        ::curl_easy_cleanup(curl);
    }

    ::fclose(file);

    return result;
#else
    (void)path;
    return false;
#endif
}

void cCurl::deleteFile()
{
#if defined(CURL_SUPPORT)
    char* path = m_path.data();
    if (path != nullptr)
    {
        ::unlink(path);

        char* n = ::strrchr(path, '/');
        if (n != nullptr)
        {
            n[0] = 0;
        }

        ::rmdir(path);

        m_path.clear();
    }
#endif
}

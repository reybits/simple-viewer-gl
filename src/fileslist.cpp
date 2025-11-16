/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "fileslist.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <unistd.h>

cFilesList::cFilesList(bool allValid, bool recursive)
    : m_allValid(allValid)
    , m_recursive(recursive)
{
}

namespace
{
    std::string GetBaseDir(const char* path)
    {
        std::string dir = path;
        size_t pos = dir.find_last_of('/');
        if (std::string::npos != pos)
        {
            dir = dir.substr(0, pos);
        }
        else
        {
            dir = ".";
        }

        return dir;
    }

    int Filter(const dirent* dir)
    {
        // skip . and ..
        auto p = dir->d_name;
        return (p[0] == '.' && (p[1] == '\0' || (p[1] == '.' && p[2] == '\0'))) ? 0 : 1;
    }

} // namespace

void cFilesList::parseDirectory(const std::string& current)
{
    const auto localCopy = current;
    auto path = localCopy.c_str();

    scanDirectory(GetBaseDir(path));

    sortList();
    locateFile(path);
}

void cFilesList::parseDir()
{
    const auto count = m_files.size();
    if (count == 1)
    {
        const auto& current = m_files[m_position].path;
        parseDirectory(current);
    }
}

void cFilesList::addFile(const char* path)
{
    const auto fullPath = ::realpath(path, nullptr);
    if (fullPath != nullptr)
    {
        path = fullPath;
    }

    if (isValidExt(path) == true)
    {
        auto it = std::find_if(m_files.begin(), m_files.end(), [path](const sFile& file) {
            return file.path == path;
        });
        if (it == m_files.end())
        {
            m_files.push_back({ false, path });
        }
    }
    else if (m_files.size() == 0)
    {
        parseDirectory(path);
    }

    if (fullPath != nullptr)
    {
        ::free(fullPath);
    }
}

void cFilesList::sortList()
{
    std::sort(m_files.begin(), m_files.end(), [](const sFile& a, const sFile& b) -> bool {
        std::string aa(a.path);
        std::string bb(b.path);
        std::transform(aa.begin(), aa.end(), aa.begin(), ::tolower);
        std::transform(bb.begin(), bb.end(), bb.begin(), ::tolower);
        return aa < bb;
    });

    auto last = std::unique(m_files.begin(), m_files.end(), [](const sFile& a, const sFile& b) {
        return a.path == b.path;
    });
    m_files.erase(last, m_files.end());

#if 0
    ::printf("Sorted unique file %u:\n", (uint32_t)m_files.size());
    for (const auto& f : m_files)
    {
        ::printf("'%s'\n", f.path.c_str());
    }
#endif
}

void cFilesList::locateFile(const char* path)
{
    const auto fullPath = ::realpath(path, nullptr);
    if (fullPath != nullptr)
    {
        path = fullPath;
    }

    const std::string name = path;
    const auto len = name.length();
    for (size_t i = 0, count = m_files.size(); i < count; i++)
    {
        const auto& file = m_files[i];
        const auto slen = file.path.length();
        if (slen >= len && file.path.substr(slen - len, len) == name)
        {
            m_position = i;
            break;
        }
    }
}

void cFilesList::scanDirectory(const std::string& root)
{
    // ::printf("scan: '%s'\n", root.c_str());

    dirent** namelist;
    int n = ::scandir(root.c_str(), &namelist, Filter, alphasort);
    if (n >= 0)
    {
        while (n--)
        {
            std::string path(root);
            path += "/";
            path += namelist[n]->d_name;

            // skip non non readable files/dirs
            DIR* dir = ::opendir(path.c_str());
            if (dir != nullptr)
            {
                ::closedir(dir);
                if (m_recursive == true)
                {
                    scanDirectory(path);
                }
            }
            else if (isValidExt(path.c_str()) == true)
            {
                m_files.push_back({ false, path });
            }

            ::free(namelist[n]);
        }

        ::free(namelist);
    }
}

namespace
{
    template <typename T = size_t>
    constexpr T length(const char* const str, T size = 0) noexcept
    {
        return *str != 0
            ? length<T>(str + 1, size + 1)
            : size;
    }

    struct Ext
    {
        const size_t size;
        const char* ext;
    };

    constexpr Ext MakeExt(const char* ext)
    {
        return { length(ext), ext };
    }

    constexpr Ext ValidExts[] = {
        // external loaders

        // openexr
        MakeExt(".exr"),
        // libjpeg
        MakeExt(".jpeg"),
        MakeExt(".jpg"),
        MakeExt(".jpe"),
        MakeExt(".jfif"),
        // openjpeg
        MakeExt(".jp2"),
        // libtiff
        MakeExt(".tiff"),
        MakeExt(".tif"),
        // libpng
        MakeExt(".png"),
        // giflib
        MakeExt(".gif"),
        // webp
        MakeExt(".webp"),
        // imlib2
        MakeExt(".lbm"),
        MakeExt(".id3"),
        MakeExt(".argb"),

        // internal loaders

        // internal age
        MakeExt(".age"),
        // internal raw
        MakeExt(".raw"),
        // internal pvr
        MakeExt(".pvr"),
        MakeExt(".pvrtc"),
        MakeExt(".pvr.gz"),
        MakeExt(".pvr.ccz"),
        // BMP
        MakeExt(".bmp"),
        // X Pixmap
        MakeExt(".xpm"),
        // Adobe PSD
        MakeExt(".psd"),
        // Adobe Illustrator
        MakeExt(".ai"),
        // Encapsulated PostScript
        MakeExt(".eps"),
        // ICO
        MakeExt(".ico"),
        // Apple Icon Image
        MakeExt(".icns"),
        // X Window Dump
        MakeExt(".xwd"),
        // GIMP XCF format
        MakeExt(".xcf"),
        // Microsoft DDS
        MakeExt(".dds"),
        // SVG
        MakeExt(".svg"),
        // Truevision Advanced Raster Graphics Adapter
        MakeExt(".tga"),
        MakeExt(".targa"),
        MakeExt(".tpic"),
        // Netpbm
        MakeExt(".pnm"),
        MakeExt(".pbm"),
        MakeExt(".pgm"),
        MakeExt(".ppm"),
        // ZX-Spectrum
        MakeExt(".scr"),
        MakeExt(".atr"),
        MakeExt(".bsc"),
        MakeExt(".ifl"),
        MakeExt(".bmc4"),
        MakeExt(".mc"),
        MakeExt(".s"),
        MakeExt(".$c"),
        MakeExt(".mg1"),
        MakeExt(".mg2"),
        MakeExt(".mg4"),
        MakeExt(".mg8"),
        MakeExt(".img"),
        MakeExt(".mgs"),
    };

} // namespace

bool cFilesList::isValidExt(const char* path)
{
    if (m_allValid)
    {
        return true;
    }

    std::string tr = path;
    std::transform(tr.begin(), tr.end(), tr.begin(), ::tolower);

    auto ptr = tr.c_str();
    const auto size = tr.length();

    for (auto& e : ValidExts)
    {
        if (size >= e.size && ::memcmp(ptr + size - e.size, e.ext, e.size) == 0)
        {
            return true;
        }
    }

    return false;
}

const char* cFilesList::getName(int delta)
{
    if (delta != 0)
    {
        parseDir();
    }

    const auto count = m_files.size();
    if (count > 0)
    {
        m_position = (m_position + count + delta) % count;
        return m_files[m_position].path.c_str();
    }

    return nullptr;
}

const char* cFilesList::getFirstName()
{
    parseDir();

    const auto count = m_files.size();
    if (count > 0)
    {
        m_position = 0;
        return m_files[m_position].path.c_str();
    }

    return nullptr;
}

const char* cFilesList::getLastName()
{
    parseDir();

    const auto count = m_files.size();
    if (count > 0)
    {
        m_position = count - 1;
        return m_files[m_position].path.c_str();
    }

    return nullptr;
}

void cFilesList::toggleDeletionMark()
{
    if (m_position < m_files.size())
    {
        auto& file = m_files[m_position];
        file.deletionMark = !file.deletionMark;
    }
}

bool cFilesList::isMarkedForDeletion() const
{
    if (m_position < m_files.size())
    {
        auto& file = m_files[m_position];
        return file.deletionMark;
    }
    return false;
}

void cFilesList::removeMarkedFromDisk()
{
    for (size_t i = 0, size = m_files.size(); i < size;)
    {
        const auto& file = m_files[i];
        if (file.deletionMark)
        {
            const auto path = file.path.c_str();
            if (::unlink(path) == 0)
            {
                ::printf("(II) File '%s' has been removed from disk.\n", path);
            }

            if (i < m_position)
            {
                m_position--;
            }

            m_files.erase(m_files.begin() + i);

            size--;
        }
        else
        {
            i++;
        }
    }
}

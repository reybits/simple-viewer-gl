/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include <string>
#include <vector>

class cFilesList final
{
public:
    cFilesList(bool allValid, bool recursive = false);

    void addFile(const char* path);
    void sortList();
    void locateFile(const char* path);

    const char* getName(int delta = 0);
    const char* getFirstName();
    const char* getLastName();

    void toggleDeletionMark();
    bool isMarkedForDeletion() const;
    void removeMarkedFromDisk();

    size_t getCount() const
    {
        return m_files.size();
    }

    size_t getIndex() const
    {
        return m_position;
    }

private:
    void parseDirectory(const std::string& current);
    void parseDir();
    void scanDirectory(const std::string& root);
    bool isValidExt(const char* path);

private:
    const bool m_allValid;
    const bool m_recursive;

private:
    size_t m_position = 0; // current position in list

    struct sFile
    {
        bool deletionMark;
        std::string path;
    };
    std::vector<sFile> m_files;
};

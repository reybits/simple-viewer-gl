/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Popup.h"

#include <cstddef>
#include <string>

struct sConfig;

class cInfoBar final : public cPopup
{
public:
    explicit cInfoBar(const sConfig& config);

    void render() override;

    void setFileName(const char* path);
    void setFormat(const char* type);
    void setDimensions(unsigned width, unsigned height, unsigned bpp);
    void setScale(float scale);
    void setFileIndex(unsigned index, unsigned count);
    void setSubImage(unsigned current, unsigned images);
    void setMemory(long fileSize, size_t memSize);

private:
    std::string getFilename(const char* path) const;
    std::string shortenFilename(const std::string& path) const;

private:
    const sConfig& m_config;

    std::string m_fileName;
    std::string m_format = "unknown";
    std::string m_dimensions;
    std::string m_scale;
    std::string m_fileIndex;
    std::string m_subImage;
    std::string m_memory;
};

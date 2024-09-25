/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include "popup.h"

#include <string>

struct sConfig;

class cInfoBar final : public cPopup
{
public:
    explicit cInfoBar(const sConfig& config);

    void render() override;

    float getHeight() const
    {
        return m_height;
    }

    struct sInfo
    {
        const char* path = nullptr;
        const char* type = nullptr;
        unsigned index = 0;
        unsigned width = 0;
        unsigned height = 0;
        unsigned bpp = 0;
        float scale = 0.0f;
        unsigned images = 0;
        unsigned current = 0;
        long file_size = 0;
        size_t mem_size = 0;
        unsigned files_count = 0;
    };

    void setInfo(const sInfo& p);

private:
    const char* getHumanSize(float& size);
    const std::string getFilename(const char* path) const;
    const std::string shortenFilename(const std::string& path) const;

private:
    const sConfig& m_config;

    std::string m_bottominfo;

    float m_height = 0.0f;
};

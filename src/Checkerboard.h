/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Quad.h"

#include <memory>

struct sConfig;

class cCheckerboard final
{
public:
    explicit cCheckerboard(const sConfig& config);
    void init();

    void render();

private:
    const sConfig& m_config;

    std::unique_ptr<cQuad> m_cb;
};

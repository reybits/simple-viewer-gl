/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include <memory>

class cQuad;

class cDeletionMark final
{
public:
    void init();

    void render();

private:
    std::unique_ptr<cQuad> m_image;
};

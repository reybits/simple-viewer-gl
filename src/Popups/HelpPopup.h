/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "popup.h"

class cHelpPopup final : public cPopup
{
public:
    void render() override;

    void show(bool show)
    {
        m_isVisible = show;
    }

    bool isVisible() const
    {
        return m_isVisible;
    }

private:
    bool m_isVisible = false;
};

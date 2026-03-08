/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

class cPopup
{
public:
    virtual ~cPopup() = default;

    virtual void render() = 0;
};

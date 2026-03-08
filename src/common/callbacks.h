/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

struct sBitmapDescription;

class iCallbacks
{
public:
    virtual void startLoading() = 0;
    virtual void onBitmapAllocated(const sBitmapDescription& desc) = 0;
    virtual void doProgress(float progress) = 0;
    virtual void endLoading() = 0;
};

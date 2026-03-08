/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Formats/Format.h"

class cNotAvailable final : public cFormat
{
public:
    cNotAvailable();

    bool isSupported(cFile& /*file*/, Buffer& /*buffer*/) const override
    {
        return true;
    }

private:
    bool LoadImpl(const char* filename, sBitmapDescription& desc) override;
};

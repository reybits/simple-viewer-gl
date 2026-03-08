/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

class cZlibDecoder final
{
public:
    cZlibDecoder();
    ~cZlibDecoder();

    unsigned decode(const unsigned char* packed, unsigned packedSize, unsigned char* const out, unsigned outSize);
};

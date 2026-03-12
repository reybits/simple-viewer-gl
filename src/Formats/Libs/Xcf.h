/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

class cFile;
struct sChunkData;
struct sImageInfo;

namespace xcf
{
    bool import(cFile& file, sChunkData& chunk, sImageInfo& info);
}

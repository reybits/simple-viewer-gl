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

bool import_xcf(cFile& file, sChunkData& chunk, sImageInfo& info);

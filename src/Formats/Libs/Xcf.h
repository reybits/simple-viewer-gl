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
struct sBitmapDescription;

bool import_xcf(cFile& file, sBitmapDescription& desc);

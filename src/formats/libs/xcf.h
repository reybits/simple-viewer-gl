/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

class cFile;
struct sBitmapDescription;

bool import_xcf(cFile& file, sBitmapDescription& desc);

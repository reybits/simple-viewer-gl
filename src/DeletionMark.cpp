/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "DeletionMark.h"
#include "Quad.h"

#include "Assets/img-na.c"

void cDeletionMark::init()
{
    m_image.reset(new cQuad(imgNa.width, imgNa.height,
                            imgNa.pixel_data,
                            imgNa.bytes_per_pixel == 3
                                ? ePixelFormat::RGB
                                : ePixelFormat::RGBA));
}

void cDeletionMark::render()
{
    m_image->render({ 10.0f, 10.0f });
}

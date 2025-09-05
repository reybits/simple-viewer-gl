/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include "types/color.h"
#include "types/rect.h"
#include "types/vector.h"

#include <GLFW/glfw3.h>

struct sVertex
{
    GLfloat x, y;
    GLfloat tx, ty;
    cColor color;
};

struct sLine
{
    GLuint tex = 0;
    sVertex v[2];
};

struct sQuad
{
    GLuint tex = 0;
    sVertex v[4];
};

class cRenderer
{
public:
    static void init(GLFWwindow* window, uint32_t maxTextureSize);
    static void shutdown();

    static GLFWwindow* getWindow();

    static void pushState();
    static void popState();

    static void enableMipmap(bool enable);
    static bool isMipmapEnabled();

    static GLuint createTexture();
    static void setData(GLuint tex, const uint8_t* data, uint32_t w, uint32_t h, GLenum format);
    static void deleteTexture(GLuint tex);
    static GLuint getCurrentTexture();
    static void bindTexture(GLuint tex);

    static uint32_t calculateTextureSize(uint32_t size);
    static void setColor(sLine* line, const cColor& color);
    static void setColor(sQuad* quad, const cColor& color);

    static void beginFrame();
    static void endFrame();

    static void render(const sLine& quad);
    static void render(const sQuad& quad);

    static void setViewportSize(const Vectori& size);
    static const Vectori& getViewportSize();

    static void resetGlobals();
    static void setGlobals(const Vectorf& offset, int angle, float zoom);

    static const Rectf& getRect();
    static float getZoom();
    static int getAngle();

    static bool checkError(const char* msg, const char* file, int line);
};

#define glCheckError(msg)                               \
    do                                                  \
    {                                                   \
        cRenderer::checkError(msg, __FILE__, __LINE__); \
    } while (0)

#define GL(glFunction)                                          \
    do                                                          \
    {                                                           \
        glFunction;                                             \
        cRenderer::checkError(#glFunction, __FILE__, __LINE__); \
    } while (0)

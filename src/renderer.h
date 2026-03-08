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

#include <glad/glad.h>

struct GLFWwindow;

// Legacy format constants removed in GL 3.3 Core but still used by format readers.
// render::setData() maps these to Core-compatible formats (GL_RED + swizzle).
#ifndef GL_LUMINANCE
#define GL_LUMINANCE 0x1909
#endif
#ifndef GL_LUMINANCE_ALPHA
#define GL_LUMINANCE_ALPHA 0x190A
#endif

struct Vertex
{
    GLfloat x, y;
    GLfloat tx, ty;
    cColor color;
};

struct Line
{
    GLuint tex = 0;
    Vertex v[2];
};

struct Quad
{
    GLuint tex = 0;
    Vertex v[4];
};

namespace render
{
    void init(GLFWwindow* window, uint32_t maxTextureSize);
    void shutdown();

    GLFWwindow* getWindow();

    void pushState();
    void popState();

    GLuint createTexture();
    void setData(GLuint tex, const uint8_t* data, uint32_t w, uint32_t h, GLenum format);
    void setCompressedData(GLuint tex, const uint8_t* data, uint32_t w, uint32_t h, GLenum internalFormat, uint32_t dataSize);
    void deleteTexture(GLuint tex);
    GLuint getCurrentTexture();
    void bindTexture(GLuint tex);

    uint32_t calculateTextureSize(uint32_t size);
    void setColor(Line* line, const cColor& color);
    void setColor(Quad* quad, const cColor& color);

    void beginFrame();
    void endFrame();

    void setTextureFilter(GLuint tex, GLenum minFilter, GLenum magFilter);
    void setTextureWrap(GLuint tex, GLenum wrap);

    void render(const Line& line);
    void render(const Quad& quad);
    void renderLines(const Vertex* vertices, uint32_t vertexCount);

    void setClearColor(float r, float g, float b, float a);
    void clear();

    void setViewportSize(const Vectori& size);
    const Vectori& getViewportSize();

    void resetGlobals();
    void setGlobals(const Vectorf& offset, int angle, float zoom);

    const Rectf& getRect();
    float getZoom();
    int getAngle();

    bool checkError(const char* msg, const char* file, int line);

} // namespace render

#define GL(glFunction)                                       \
    do                                                       \
    {                                                        \
        glFunction;                                          \
        render::checkError(#glFunction, __FILE__, __LINE__); \
    } while (0)

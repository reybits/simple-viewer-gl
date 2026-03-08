/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
*  Texture filtration
*  by Timo Suoranta <tksuoran@gmail.com>
*
\**********************************************/

#include "renderer.h"
#include "types/matrix.h"
#include "types/vector.h"

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <vector>

namespace
{
    GLFWwindow* Window = nullptr;
    Rectf ViewRect;
    float ViewZoom = 1.0f;
    int ViewAngle = 0;
    Vectori ViewportSize;
    GLuint CurrentTextureId = 0;
    uint32_t TextureSizeLimit = 1024;

    // Shader programs
    GLuint TexturedProgram = 0;
    GLuint ColoredProgram = 0;

    // Uniform locations
    GLint TexturedProjLoc = -1;
    GLint TexturedTexLoc = -1;
    GLint ColoredProjLoc = -1;

    // VAO/VBO/IBO
    GLuint Vao = 0;
    GLuint Vbo = 0;
    GLuint Ibo = 0;

    // Current projection matrix
    Matrix4 Projection = Matrix4::Identity();

    struct GLState
    {
        GLuint texture = 0;
        GLint viewport[4] = { 0 };
        GLint scissorBox[4] = { 0 };
        GLboolean blendEnabled = GL_FALSE;
        GLboolean scissorEnabled = GL_FALSE;
        GLint arrayBuffer = 0;
        GLint vertexArray = 0;
        GLint program = 0;
        Matrix4 projection;
    };

    std::vector<GLState> GLStates;

    constexpr const char* VertexShaderSource = R"glsl(
#version 330 core
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;
layout(location = 2) in vec4 aColor;
uniform mat4 uProjection;
out vec2 vTexCoord;
out vec4 vColor;
void main()
{
    gl_Position = uProjection * vec4(aPos, 0.0, 1.0);
    vTexCoord = aTexCoord;
    vColor = aColor;
}
)glsl";

    constexpr const char* TexturedFragSource = R"glsl(
#version 330 core
in vec2 vTexCoord;
in vec4 vColor;
uniform sampler2D uTexture;
out vec4 FragColor;
void main()
{
    FragColor = texture(uTexture, vTexCoord) * vColor;
}
)glsl";

    constexpr const char* ColoredFragSource = R"glsl(
#version 330 core
in vec2 vTexCoord;
in vec4 vColor;
out vec4 FragColor;
void main()
{
    FragColor = vColor;
}
)glsl";

    GLuint compileShader(GLenum type, const char* source)
    {
        GLuint shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        GLint success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char log[512];
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            std::printf("(EE) Shader compilation error: %s\n", log);
        }
        return shader;
    }

    GLuint createProgram(const char* vertSrc, const char* fragSrc)
    {
        GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
        GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);

        GLuint program = glCreateProgram();
        glAttachShader(program, vert);
        glAttachShader(program, frag);
        glLinkProgram(program);

        GLint success = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success)
        {
            char log[512];
            glGetProgramInfoLog(program, sizeof(log), nullptr, log);
            std::printf("(EE) Program link error: %s\n", log);
        }

        glDeleteShader(vert);
        glDeleteShader(frag);
        return program;
    }

} // namespace

void render::init(GLFWwindow* window, uint32_t maxTextureSize)
{
    Window = window;
    CurrentTextureId = 0;

    int maxSize = static_cast<int>(maxTextureSize);
    GL(glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxSize));
    TextureSizeLimit = std::min<uint32_t>(static_cast<uint32_t>(maxSize), maxTextureSize);

    // Create shader programs
    TexturedProgram = createProgram(VertexShaderSource, TexturedFragSource);
    TexturedProjLoc = glGetUniformLocation(TexturedProgram, "uProjection");
    TexturedTexLoc = glGetUniformLocation(TexturedProgram, "uTexture");

    ColoredProgram = createProgram(VertexShaderSource, ColoredFragSource);
    ColoredProjLoc = glGetUniformLocation(ColoredProgram, "uProjection");

    // Create VAO
    GL(glGenVertexArrays(1, &Vao));
    GL(glBindVertexArray(Vao));

    // Create VBO (streaming)
    GL(glGenBuffers(1, &Vbo));
    GL(glBindBuffer(GL_ARRAY_BUFFER, Vbo));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * 4, nullptr, GL_DYNAMIC_DRAW));

    // Create IBO with quad indices
    const uint16_t indices[6] = { 0, 1, 2, 0, 2, 3 };
    GL(glGenBuffers(1, &Ibo));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, Ibo));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW));

    // Setup vertex attributes: pos(2f), texcoord(2f), color(4ub)
    // Vertex layout: x,y (8 bytes), tx,ty (8 bytes), color rgba (4 bytes) = 20 bytes
    GL(glEnableVertexAttribArray(0));
    GL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, x))));
    GL(glEnableVertexAttribArray(1));
    GL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, tx))));
    GL(glEnableVertexAttribArray(2));
    GL(glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color))));

    GL(glBindVertexArray(0));
}

void render::shutdown()
{
    if (Vao)
    {
        glDeleteVertexArrays(1, &Vao);
        Vao = 0;
    }
    if (Vbo)
    {
        glDeleteBuffers(1, &Vbo);
        Vbo = 0;
    }
    if (Ibo)
    {
        glDeleteBuffers(1, &Ibo);
        Ibo = 0;
    }
    if (TexturedProgram)
    {
        glDeleteProgram(TexturedProgram);
        TexturedProgram = 0;
    }
    if (ColoredProgram)
    {
        glDeleteProgram(ColoredProgram);
        ColoredProgram = 0;
    }

    Window = nullptr;
    ViewZoom = 1.0f;
    ViewAngle = 0;
    CurrentTextureId = 0;
    TextureSizeLimit = 1024;

    assert(GLStates.empty() == true);
}

void render::beginFrame()
{
    GL(glEnable(GL_BLEND));
    GL(glDisable(GL_DEPTH_TEST));
    GL(glDisable(GL_CULL_FACE));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    GL(glClearColor(0, 0, 0, 0));
    GL(glClear(GL_COLOR_BUFFER_BIT));

    GL(glActiveTexture(GL_TEXTURE0));
    GL(glBindVertexArray(Vao));
    GL(glBindBuffer(GL_ARRAY_BUFFER, Vbo));
}

void render::endFrame()
{
    GL(glBindVertexArray(0));
}

GLFWwindow* render::getWindow()
{
    return Window;
}

void render::pushState()
{
    GLState state;
    state.texture = CurrentTextureId;
    GL(glGetIntegerv(GL_VIEWPORT, state.viewport));
    GL(glGetIntegerv(GL_SCISSOR_BOX, state.scissorBox));
    state.blendEnabled = glIsEnabled(GL_BLEND);
    state.scissorEnabled = glIsEnabled(GL_SCISSOR_TEST);
    GL(glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &state.arrayBuffer));
    GL(glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &state.vertexArray));
    GL(glGetIntegerv(GL_CURRENT_PROGRAM, &state.program));
    state.projection = Projection;

    GLStates.push_back(state);
}

void render::popState()
{
    assert(GLStates.empty() == false);

    auto& state = GLStates.back();

    GL(glUseProgram(static_cast<GLuint>(state.program)));
    GL(glBindVertexArray(static_cast<GLuint>(state.vertexArray)));
    GL(glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(state.arrayBuffer)));

    GL(glViewport(state.viewport[0], state.viewport[1], static_cast<GLsizei>(state.viewport[2]), static_cast<GLsizei>(state.viewport[3])));
    GL(glScissor(state.scissorBox[0], state.scissorBox[1], static_cast<GLsizei>(state.scissorBox[2]), static_cast<GLsizei>(state.scissorBox[3])));

    if (state.blendEnabled)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);

    if (state.scissorEnabled)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    Projection = state.projection;
    bindTexture(state.texture);

    GLStates.pop_back();
}

void render::setData(GLuint tex, const uint8_t* data, uint32_t w, uint32_t h, GLenum format)
{
    bindTexture(tex);

    if (tex != 0 && data != nullptr)
    {
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

        GLenum type = GL_UNSIGNED_BYTE;
        GLint internalFormat = static_cast<GLint>(format);
        GLenum uploadFormat = format;

        if (format == GL_RGB || format == GL_BGR)
        {
            internalFormat = GL_RGB8;
            uploadFormat = format;
            type = GL_UNSIGNED_BYTE;
        }
        else if (format == GL_RGBA || format == GL_BGRA)
        {
            internalFormat = GL_RGBA8;
            uploadFormat = format;
            type = GL_UNSIGNED_BYTE;
        }
        else if (format == GL_UNSIGNED_SHORT_4_4_4_4)
        {
            internalFormat = GL_RGBA4;
            uploadFormat = GL_RGBA;
            type = GL_UNSIGNED_SHORT_4_4_4_4;
        }
        else if (format == GL_UNSIGNED_SHORT_5_6_5)
        {
            internalFormat = GL_RGB8;
            uploadFormat = GL_RGB;
            type = GL_UNSIGNED_SHORT_5_6_5;
        }
        else if (format == GL_UNSIGNED_SHORT_5_5_5_1)
        {
            internalFormat = GL_RGB5_A1;
            uploadFormat = GL_RGBA;
            type = GL_UNSIGNED_SHORT_5_5_5_1;
        }
        else if (format == GL_LUMINANCE)
        {
            // GL_LUMINANCE removed in Core — use GL_RED + swizzle
            internalFormat = GL_R8;
            uploadFormat = GL_RED;
            type = GL_UNSIGNED_BYTE;
        }
        else if (format == GL_LUMINANCE_ALPHA)
        {
            internalFormat = GL_RG8;
            uploadFormat = GL_RG;
            type = GL_UNSIGNED_BYTE;
        }
        else if (format == GL_ALPHA)
        {
            internalFormat = GL_R8;
            uploadFormat = GL_RED;
            type = GL_UNSIGNED_BYTE;
        }

        GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 4));
        GL(glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, uploadFormat, type, data));

        // Set swizzle masks for legacy format compatibility
        if (format == GL_LUMINANCE)
        {
            GLint swizzle[] = { GL_RED, GL_RED, GL_RED, GL_ONE };
            GL(glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle));
        }
        else if (format == GL_LUMINANCE_ALPHA)
        {
            GLint swizzle[] = { GL_RED, GL_RED, GL_RED, GL_GREEN };
            GL(glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle));
        }
        else if (format == GL_ALPHA)
        {
            GLint swizzle[] = { GL_ZERO, GL_ZERO, GL_ZERO, GL_RED };
            GL(glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle));
        }
    }
}

void render::setCompressedData(GLuint tex, const uint8_t* data, uint32_t w, uint32_t h, GLenum internalFormat, uint32_t dataSize)
{
    bindTexture(tex);

    if (tex != 0 && data != nullptr)
    {
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
        GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));

        GL(glCompressedTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, dataSize, data));
    }
}

GLuint render::createTexture()
{
    GLuint tex = 0;
    GL(glGenTextures(1, &tex));

    return tex;
}

bool render::checkError(const char* msg, const char* file, int line)
{
    auto result = false;
    for (auto e = glGetError(); e != GL_NO_ERROR; e = glGetError())
    {
        result = true;
        printf("(EE) %s error 0x%x at %s:%d\n", msg, e, file, line);
    }

    return result;
}

void render::deleteTexture(GLuint tex)
{
    if (CurrentTextureId == tex)
    {
        bindTexture(0);
    }
    GL(glDeleteTextures(1, &tex));
}

GLuint render::getCurrentTexture()
{
    return CurrentTextureId;
}

void render::bindTexture(GLuint tex)
{
    if (CurrentTextureId != tex)
    {
        CurrentTextureId = tex;
        GL(glBindTexture(GL_TEXTURE_2D, tex));
    }
}

uint32_t render::calculateTextureSize(uint32_t size)
{
    return std::min<uint32_t>(TextureSizeLimit, size);
}

void render::setColor(Line* line, const cColor& color)
{
    for (uint32_t i = 0; i < 2; i++)
    {
        line->v[i].color = color;
    }
}

void render::setColor(Quad* quad, const cColor& color)
{
    for (uint32_t i = 0; i < 4; i++)
    {
        quad->v[i].color = color;
    }
}

void render::render(const Line& line)
{
    bindTexture(line.tex);

    GLuint program = (line.tex != 0) ? TexturedProgram : ColoredProgram;
    GLint projLoc = (line.tex != 0) ? TexturedProjLoc : ColoredProjLoc;

    GL(glUseProgram(program));
    GL(glUniformMatrix4fv(projLoc, 1, GL_FALSE, Projection.m));
    if (line.tex != 0)
    {
        GL(glUniform1i(TexturedTexLoc, 0));
    }

    GL(glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex) * 2, line.v));
    GL(glDrawArrays(GL_LINES, 0, 2));
}

void render::render(const Quad& quad)
{
    bindTexture(quad.tex);

    GLuint program = (quad.tex != 0) ? TexturedProgram : ColoredProgram;
    GLint projLoc = (quad.tex != 0) ? TexturedProjLoc : ColoredProjLoc;

    GL(glUseProgram(program));
    GL(glUniformMatrix4fv(projLoc, 1, GL_FALSE, Projection.m));
    if (quad.tex != 0)
    {
        GL(glUniform1i(TexturedTexLoc, 0));
    }

    GL(glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex) * 4, quad.v));
    GL(glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, nullptr));
}

void render::setClearColor(float r, float g, float b, float a)
{
    GL(glClearColor(r, g, b, a));
}

void render::clear()
{
    GL(glClear(GL_COLOR_BUFFER_BIT));
}

const Vectori& render::getViewportSize()
{
    return ViewportSize;
}

void render::setViewportSize(const Vectori& size)
{
    GL(glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y)));
    ViewportSize = size;
}

void render::resetGlobals()
{
    ViewRect = { { 0.0f, 0.0f }, { static_cast<float>(ViewportSize.x), static_cast<float>(ViewportSize.y) } };
    ViewZoom = 1.0f;

    Projection = Matrix4::Ortho(
        0.0f,
        static_cast<float>(ViewportSize.x),
        static_cast<float>(ViewportSize.y),
        0.0f,
        -1.0f, 1.0f);
}

const Rectf& render::getRect()
{
    return ViewRect;
}

float render::getZoom()
{
    return ViewZoom;
}

int render::getAngle()
{
    return ViewAngle;
}

void render::setGlobals(const Vectorf& offset, int angle, float zoom)
{
    const float z = 1.0f / zoom;
    const float w = ViewportSize.x * z;
    const float h = ViewportSize.y * z;

    const float x = offset.x - w * 0.5f;
    const float y = offset.y - h * 0.5f;

    ViewRect = { { x, y }, { x + w, y + h } };
    ViewZoom = zoom;
    ViewAngle = angle;

    auto ortho = Matrix4::Ortho(x, x + w, y + h, y, -1.0f, 1.0f);
    auto rotate = Matrix4::RotateZ(static_cast<float>(-angle));
    Projection = ortho * rotate;
}

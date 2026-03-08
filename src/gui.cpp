/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "gui.h"
#include "DroidSans.h"
#include "renderer.h"
#include "types/matrix.h"
#include "window.h"

#include "common/timing.h"

#include <GLFW/glfw3.h>
#include <imgui/imgui.h>

namespace
{
    GLuint ImGuiShaderProgram = 0;
    GLint ImGuiProjLoc = -1;
    GLint ImGuiTexLoc = -1;
    GLuint ImGuiVao = 0;
    GLuint ImGuiVbo = 0;
    GLuint ImGuiIbo = 0;

    constexpr const char* ImGuiVertexShader = R"glsl(
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

    constexpr const char* ImGuiFragmentShader = R"glsl(
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
            printf("(EE) ImGui shader error: %s\n", log);
        }
        return shader;
    }

    void initImGuiGL()
    {
        GLuint vert = compileShader(GL_VERTEX_SHADER, ImGuiVertexShader);
        GLuint frag = compileShader(GL_FRAGMENT_SHADER, ImGuiFragmentShader);

        ImGuiShaderProgram = glCreateProgram();
        glAttachShader(ImGuiShaderProgram, vert);
        glAttachShader(ImGuiShaderProgram, frag);
        glLinkProgram(ImGuiShaderProgram);
        glDeleteShader(vert);
        glDeleteShader(frag);

        ImGuiProjLoc = glGetUniformLocation(ImGuiShaderProgram, "uProjection");
        ImGuiTexLoc = glGetUniformLocation(ImGuiShaderProgram, "uTexture");

        GL(glGenVertexArrays(1, &ImGuiVao));
        GL(glGenBuffers(1, &ImGuiVbo));
        GL(glGenBuffers(1, &ImGuiIbo));
    }

    void shutdownImGuiGL()
    {
        if (ImGuiVao) { glDeleteVertexArrays(1, &ImGuiVao); ImGuiVao = 0; }
        if (ImGuiVbo) { glDeleteBuffers(1, &ImGuiVbo); ImGuiVbo = 0; }
        if (ImGuiIbo) { glDeleteBuffers(1, &ImGuiIbo); ImGuiIbo = 0; }
        if (ImGuiShaderProgram) { glDeleteProgram(ImGuiShaderProgram); ImGuiShaderProgram = 0; }
    }

    void updateTexture(ImTextureData* textureData)
    {
        if (textureData->Status == ImTextureStatus_WantCreate)
        {
            IM_ASSERT(textureData->TexID == 0 && textureData->BackendUserData == nullptr);
            IM_ASSERT(textureData->Format == ImTextureFormat_RGBA32);
            auto tex = render::createTexture();

            render::setTextureFilter(tex, GL_LINEAR, GL_LINEAR);
            render::setTextureWrap(tex, GL_CLAMP_TO_EDGE);
            GL(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
            GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
            auto pixels = textureData->GetPixels();
            GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureData->Width, textureData->Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels));

            textureData->SetTexID(tex);
            textureData->SetStatus(ImTextureStatus_OK);
        }
        else if (textureData->Status == ImTextureStatus_WantUpdates)
        {
            render::bindTexture(textureData->TexID);
            GL(glPixelStorei(GL_UNPACK_ROW_LENGTH, textureData->Width));
            GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
            for (auto& r : textureData->Updates)
            {
                GL(glTexSubImage2D(GL_TEXTURE_2D, 0, r.x, r.y, r.w, r.h, GL_RGBA, GL_UNSIGNED_BYTE, textureData->GetPixelsAt(r.x, r.y)));
            }
            GL(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
            textureData->SetStatus(ImTextureStatus_OK);
        }
        else if (textureData->Status == ImTextureStatus_WantDestroy)
        {
            render::deleteTexture(textureData->TexID);
            textureData->SetTexID(ImTextureID_Invalid);
            textureData->SetStatus(ImTextureStatus_Destroyed);
        }
    }

    // -------------------------------------------------------------------------

    ImGuiKey KeyToImGuiKey(int keycode)
    {
        switch (keycode)
        {
        case GLFW_KEY_TAB:
            return ImGuiKey_Tab;
        case GLFW_KEY_LEFT:
            return ImGuiKey_LeftArrow;
        case GLFW_KEY_RIGHT:
            return ImGuiKey_RightArrow;
        case GLFW_KEY_UP:
            return ImGuiKey_UpArrow;
        case GLFW_KEY_DOWN:
            return ImGuiKey_DownArrow;
        case GLFW_KEY_PAGE_UP:
            return ImGuiKey_PageUp;
        case GLFW_KEY_PAGE_DOWN:
            return ImGuiKey_PageDown;
        case GLFW_KEY_HOME:
            return ImGuiKey_Home;
        case GLFW_KEY_END:
            return ImGuiKey_End;
        case GLFW_KEY_INSERT:
            return ImGuiKey_Insert;
        case GLFW_KEY_DELETE:
            return ImGuiKey_Delete;
        case GLFW_KEY_BACKSPACE:
            return ImGuiKey_Backspace;
        case GLFW_KEY_SPACE:
            return ImGuiKey_Space;
        case GLFW_KEY_ENTER:
            return ImGuiKey_Enter;
        case GLFW_KEY_ESCAPE:
            return ImGuiKey_Escape;
        case GLFW_KEY_APOSTROPHE:
            return ImGuiKey_Apostrophe;
        case GLFW_KEY_COMMA:
            return ImGuiKey_Comma;
        case GLFW_KEY_MINUS:
            return ImGuiKey_Minus;
        case GLFW_KEY_PERIOD:
            return ImGuiKey_Period;
        case GLFW_KEY_SLASH:
            return ImGuiKey_Slash;
        case GLFW_KEY_SEMICOLON:
            return ImGuiKey_Semicolon;
        case GLFW_KEY_EQUAL:
            return ImGuiKey_Equal;
        case GLFW_KEY_LEFT_BRACKET:
            return ImGuiKey_LeftBracket;
        case GLFW_KEY_BACKSLASH:
            return ImGuiKey_Backslash;
        case GLFW_KEY_WORLD_1:
            return ImGuiKey_Oem102;
        case GLFW_KEY_WORLD_2:
            return ImGuiKey_Oem102;
        case GLFW_KEY_RIGHT_BRACKET:
            return ImGuiKey_RightBracket;
        case GLFW_KEY_GRAVE_ACCENT:
            return ImGuiKey_GraveAccent;
        case GLFW_KEY_CAPS_LOCK:
            return ImGuiKey_CapsLock;
        case GLFW_KEY_SCROLL_LOCK:
            return ImGuiKey_ScrollLock;
        case GLFW_KEY_NUM_LOCK:
            return ImGuiKey_NumLock;
        case GLFW_KEY_PRINT_SCREEN:
            return ImGuiKey_PrintScreen;
        case GLFW_KEY_PAUSE:
            return ImGuiKey_Pause;
        case GLFW_KEY_KP_0:
            return ImGuiKey_Keypad0;
        case GLFW_KEY_KP_1:
            return ImGuiKey_Keypad1;
        case GLFW_KEY_KP_2:
            return ImGuiKey_Keypad2;
        case GLFW_KEY_KP_3:
            return ImGuiKey_Keypad3;
        case GLFW_KEY_KP_4:
            return ImGuiKey_Keypad4;
        case GLFW_KEY_KP_5:
            return ImGuiKey_Keypad5;
        case GLFW_KEY_KP_6:
            return ImGuiKey_Keypad6;
        case GLFW_KEY_KP_7:
            return ImGuiKey_Keypad7;
        case GLFW_KEY_KP_8:
            return ImGuiKey_Keypad8;
        case GLFW_KEY_KP_9:
            return ImGuiKey_Keypad9;
        case GLFW_KEY_KP_DECIMAL:
            return ImGuiKey_KeypadDecimal;
        case GLFW_KEY_KP_DIVIDE:
            return ImGuiKey_KeypadDivide;
        case GLFW_KEY_KP_MULTIPLY:
            return ImGuiKey_KeypadMultiply;
        case GLFW_KEY_KP_SUBTRACT:
            return ImGuiKey_KeypadSubtract;
        case GLFW_KEY_KP_ADD:
            return ImGuiKey_KeypadAdd;
        case GLFW_KEY_KP_ENTER:
            return ImGuiKey_KeypadEnter;
        case GLFW_KEY_KP_EQUAL:
            return ImGuiKey_KeypadEqual;
        case GLFW_KEY_LEFT_SHIFT:
            return ImGuiKey_LeftShift;
        case GLFW_KEY_LEFT_CONTROL:
            return ImGuiKey_LeftCtrl;
        case GLFW_KEY_LEFT_ALT:
            return ImGuiKey_LeftAlt;
        case GLFW_KEY_LEFT_SUPER:
            return ImGuiKey_LeftSuper;
        case GLFW_KEY_RIGHT_SHIFT:
            return ImGuiKey_RightShift;
        case GLFW_KEY_RIGHT_CONTROL:
            return ImGuiKey_RightCtrl;
        case GLFW_KEY_RIGHT_ALT:
            return ImGuiKey_RightAlt;
        case GLFW_KEY_RIGHT_SUPER:
            return ImGuiKey_RightSuper;
        case GLFW_KEY_MENU:
            return ImGuiKey_Menu;
        case GLFW_KEY_0:
            return ImGuiKey_0;
        case GLFW_KEY_1:
            return ImGuiKey_1;
        case GLFW_KEY_2:
            return ImGuiKey_2;
        case GLFW_KEY_3:
            return ImGuiKey_3;
        case GLFW_KEY_4:
            return ImGuiKey_4;
        case GLFW_KEY_5:
            return ImGuiKey_5;
        case GLFW_KEY_6:
            return ImGuiKey_6;
        case GLFW_KEY_7:
            return ImGuiKey_7;
        case GLFW_KEY_8:
            return ImGuiKey_8;
        case GLFW_KEY_9:
            return ImGuiKey_9;
        case GLFW_KEY_A:
            return ImGuiKey_A;
        case GLFW_KEY_B:
            return ImGuiKey_B;
        case GLFW_KEY_C:
            return ImGuiKey_C;
        case GLFW_KEY_D:
            return ImGuiKey_D;
        case GLFW_KEY_E:
            return ImGuiKey_E;
        case GLFW_KEY_F:
            return ImGuiKey_F;
        case GLFW_KEY_G:
            return ImGuiKey_G;
        case GLFW_KEY_H:
            return ImGuiKey_H;
        case GLFW_KEY_I:
            return ImGuiKey_I;
        case GLFW_KEY_J:
            return ImGuiKey_J;
        case GLFW_KEY_K:
            return ImGuiKey_K;
        case GLFW_KEY_L:
            return ImGuiKey_L;
        case GLFW_KEY_M:
            return ImGuiKey_M;
        case GLFW_KEY_N:
            return ImGuiKey_N;
        case GLFW_KEY_O:
            return ImGuiKey_O;
        case GLFW_KEY_P:
            return ImGuiKey_P;
        case GLFW_KEY_Q:
            return ImGuiKey_Q;
        case GLFW_KEY_R:
            return ImGuiKey_R;
        case GLFW_KEY_S:
            return ImGuiKey_S;
        case GLFW_KEY_T:
            return ImGuiKey_T;
        case GLFW_KEY_U:
            return ImGuiKey_U;
        case GLFW_KEY_V:
            return ImGuiKey_V;
        case GLFW_KEY_W:
            return ImGuiKey_W;
        case GLFW_KEY_X:
            return ImGuiKey_X;
        case GLFW_KEY_Y:
            return ImGuiKey_Y;
        case GLFW_KEY_Z:
            return ImGuiKey_Z;
        case GLFW_KEY_F1:
            return ImGuiKey_F1;
        case GLFW_KEY_F2:
            return ImGuiKey_F2;
        case GLFW_KEY_F3:
            return ImGuiKey_F3;
        case GLFW_KEY_F4:
            return ImGuiKey_F4;
        case GLFW_KEY_F5:
            return ImGuiKey_F5;
        case GLFW_KEY_F6:
            return ImGuiKey_F6;
        case GLFW_KEY_F7:
            return ImGuiKey_F7;
        case GLFW_KEY_F8:
            return ImGuiKey_F8;
        case GLFW_KEY_F9:
            return ImGuiKey_F9;
        case GLFW_KEY_F10:
            return ImGuiKey_F10;
        case GLFW_KEY_F11:
            return ImGuiKey_F11;
        case GLFW_KEY_F12:
            return ImGuiKey_F12;
        case GLFW_KEY_F13:
            return ImGuiKey_F13;
        case GLFW_KEY_F14:
            return ImGuiKey_F14;
        case GLFW_KEY_F15:
            return ImGuiKey_F15;
        case GLFW_KEY_F16:
            return ImGuiKey_F16;
        case GLFW_KEY_F17:
            return ImGuiKey_F17;
        case GLFW_KEY_F18:
            return ImGuiKey_F18;
        case GLFW_KEY_F19:
            return ImGuiKey_F19;
        case GLFW_KEY_F20:
            return ImGuiKey_F20;
        case GLFW_KEY_F21:
            return ImGuiKey_F21;
        case GLFW_KEY_F22:
            return ImGuiKey_F22;
        case GLFW_KEY_F23:
            return ImGuiKey_F23;
        case GLFW_KEY_F24:
            return ImGuiKey_F24;
        }

        return ImGuiKey_None;
    }

} // namespace

void cGui::onMousePosition(const Vectorf& pos)
{
    auto& io = ImGui::GetIO();
    io.MousePos = ImVec2(pos.x, pos.y);
}

void cGui::onMouseButton(int button, int action)
{
    if (button >= 0 && button < 3)
    {
        auto& io = ImGui::GetIO();
        io.MouseDown[button] = action != GLFW_RELEASE;
    }
}

void cGui::onScroll(const Vectorf& pos)
{
    auto& io = ImGui::GetIO();
    io.MouseWheel += pos.y;
}

void cGui::onKey(int key, int, int action)
{
    if (key >= 0 && key < ImGuiKey_COUNT)
    {
        auto& io = ImGui::GetIO();

        auto isPressed = action != GLFW_RELEASE;

        if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL)
        {
            io.AddKeyEvent(ImGuiMod_Ctrl, isPressed);
        }
        if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT)
        {
            io.AddKeyEvent(ImGuiMod_Shift, isPressed);
        }
        if (key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT)
        {
            io.AddKeyEvent(ImGuiMod_Alt, isPressed);
        }
        if (key == GLFW_KEY_LEFT_SUPER || key == GLFW_KEY_RIGHT_SUPER)
        {
            io.AddKeyEvent(ImGuiMod_Super, isPressed);
        }

        auto imguiKey = KeyToImGuiKey(key);
        if (imguiKey != ImGuiKey_None)
        {
            io.AddKeyEvent(imguiKey, isPressed);
        }
    }
}

void cGui::onChar(uint32_t c)
{
    auto& io = ImGui::GetIO();
    if (c > 0 && c < 0x10000)
    {
        io.AddInputCharacter(c);
    }
}

void cGui::init(cWindow& window)
{
    m_window = &window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto& io = ImGui::GetIO();

    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    io.SetClipboardTextFn = [](void* user_data, const char* text) {
        static_cast<cWindow*>(user_data)->setClipboardText(text);
    };

    io.GetClipboardTextFn = [](void* user_data) {
        return static_cast<cWindow*>(user_data)->getClipboardText();
    };

    io.ClipboardUserData = m_window;

    const ImWchar range[] = { 0x0020, 0xFFFF, 0 };
    io.Fonts->AddFontFromMemoryCompressedTTF(DroidSans_compressed_data, DroidSans_compressed_size, 16, nullptr, range);
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures;

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    auto& s = ImGui::GetStyle();
    s.WindowTitleAlign = { 0.5f, 0.5f };
    s.WindowRounding = 5.0f;
    s.WindowPadding = { 3.0f, 3.0f };

    initImGuiGL();
}

void cGui::shutdown()
{
    shutdownImGuiGL();
    ImGui::DestroyContext();
}

void cGui::beginFrame()
{
    auto& io = ImGui::GetIO();

    auto winSize = m_window->getWindowSize();
    auto fbSize = m_window->getFramebufferSize();
    io.DisplaySize = ImVec2(static_cast<float>(winSize.x), static_cast<float>(winSize.y));
    io.DisplayFramebufferScale = ImVec2(
        winSize.x > 0 ? (static_cast<float>(fbSize.x) / winSize.x) : 0,
        winSize.y > 0 ? (static_cast<float>(fbSize.y) / winSize.y) : 0);

    double current_time = timing::seconds();
    io.DeltaTime = m_time > 0.0 ? static_cast<float>(current_time - m_time) : (1.0f / 60.0f);
    m_time = current_time;

    ImGui::NewFrame();
}

void cGui::endFrame()
{
    ImGui::Render();

    auto drawData = ImGui::GetDrawData();

    auto& io = ImGui::GetIO();
    auto width = static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    auto height = static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (width == 0 || height == 0)
    {
        return;
    }

    // Update ImGui textures.
    if (drawData->Textures != nullptr)
    {
        for (auto tex : *drawData->Textures)
        {
            if (tex->Status != ImTextureStatus_OK)
            {
                updateTexture(tex);
            }
        }
    }

    // Save GL state
    render::pushState();

    // Setup render state
    GL(glEnable(GL_BLEND));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    GL(glDisable(GL_CULL_FACE));
    GL(glDisable(GL_DEPTH_TEST));
    GL(glDisable(GL_STENCIL_TEST));
    GL(glEnable(GL_SCISSOR_TEST));

    GL(glViewport(0, 0, width, height));

    auto projection = Matrix4::Ortho(
        drawData->DisplayPos.x,
        drawData->DisplayPos.x + drawData->DisplaySize.x,
        drawData->DisplayPos.y + drawData->DisplaySize.y,
        drawData->DisplayPos.y,
        -1.0f, 1.0f);

    GL(glUseProgram(ImGuiShaderProgram));
    GL(glUniformMatrix4fv(ImGuiProjLoc, 1, GL_FALSE, projection.m));
    GL(glUniform1i(ImGuiTexLoc, 0));

    GL(glBindVertexArray(ImGuiVao));
    GL(glBindBuffer(GL_ARRAY_BUFFER, ImGuiVbo));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ImGuiIbo));

    // Setup vertex attributes for ImDrawVert
    GL(glEnableVertexAttribArray(0));
    GL(glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), reinterpret_cast<void*>(offsetof(ImDrawVert, pos))));
    GL(glEnableVertexAttribArray(1));
    GL(glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), reinterpret_cast<void*>(offsetof(ImDrawVert, uv))));
    GL(glEnableVertexAttribArray(2));
    GL(glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), reinterpret_cast<void*>(offsetof(ImDrawVert, col))));

    const auto& clipOff = drawData->DisplayPos;
    const auto& clipScale = drawData->FramebufferScale;

    for (const auto drawList : drawData->CmdLists)
    {
        // Upload vertex/index data
        GL(glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(drawList->VtxBuffer.Size * sizeof(ImDrawVert)), drawList->VtxBuffer.Data, GL_STREAM_DRAW));
        GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLsizeiptr>(drawList->IdxBuffer.Size * sizeof(ImDrawIdx)), drawList->IdxBuffer.Data, GL_STREAM_DRAW));

        for (int idx = 0; idx < drawList->CmdBuffer.Size; idx++)
        {
            const auto pcmd = &drawList->CmdBuffer[idx];
            if (pcmd->UserCallback)
            {
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                {
                    // Re-setup state if needed
                    GL(glEnable(GL_BLEND));
                    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
                    GL(glDisable(GL_CULL_FACE));
                    GL(glDisable(GL_DEPTH_TEST));
                    GL(glEnable(GL_SCISSOR_TEST));
                    GL(glViewport(0, 0, width, height));
                    GL(glUseProgram(ImGuiShaderProgram));
                    GL(glUniformMatrix4fv(ImGuiProjLoc, 1, GL_FALSE, projection.m));
                    GL(glUniform1i(ImGuiTexLoc, 0));
                }
                else
                {
                    pcmd->UserCallback(drawList, pcmd);
                }
            }
            else
            {
                ImVec2 clipMin{ (pcmd->ClipRect.x - clipOff.x) * clipScale.x,
                                (pcmd->ClipRect.y - clipOff.y) * clipScale.y };
                ImVec2 clipMax{ (pcmd->ClipRect.z - clipOff.x) * clipScale.x,
                                (pcmd->ClipRect.w - clipOff.y) * clipScale.y };
                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                {
                    continue;
                }

                GL(glScissor(static_cast<int>(clipMin.x),
                             static_cast<int>(static_cast<float>(height) - clipMax.y),
                             static_cast<int>(clipMax.x - clipMin.x),
                             static_cast<int>(clipMax.y - clipMin.y)));

                render::bindTexture(pcmd->GetTexID());
                auto type = sizeof(ImDrawIdx) == 2
                    ? GL_UNSIGNED_SHORT
                    : GL_UNSIGNED_INT;
                GL(glDrawElements(GL_TRIANGLES, pcmd->ElemCount, type, reinterpret_cast<void*>(static_cast<intptr_t>(pcmd->IdxOffset * sizeof(ImDrawIdx)))));
            }
        }
    }

    // Restore GL state (popState restores VAO, VBO, program, scissor, etc.)
    render::popState();
}

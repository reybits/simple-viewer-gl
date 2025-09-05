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

#include <GLFW/glfw3.h>
#include <imgui/imgui.h>

namespace
{
    class RenderState final
    {
    public:
        RenderState(ImDrawData* drawData, int width, int height)
            : m_drawData(drawData)
            , m_fbWidth(width)
            , m_fbHeight(height)
        {
            cRenderer::pushState();

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
        }

        ~RenderState()
        {
            // Restore modified GL state.
            GL(glDisableClientState(GL_COLOR_ARRAY));
            GL(glDisableClientState(GL_TEXTURE_COORD_ARRAY));
            GL(glDisableClientState(GL_VERTEX_ARRAY));

            cRenderer::popState();

            GL(glMatrixMode(GL_MODELVIEW));
            GL(glPopMatrix());
            GL(glMatrixMode(GL_PROJECTION));
            GL(glPopMatrix());
        }

        void resetState()
        {
            // Setup render state:
            // alpha-blending enabled, no face culling, no depth testing,
            // scissor enabled, vertex/texcoord/color pointers, polygon fill.
            GL(glEnable(GL_BLEND));
            GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
            // In order to composite our output buffer we need to preserve alpha
            // GL(glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA));
            GL(glDisable(GL_CULL_FACE));
            GL(glDisable(GL_DEPTH_TEST));
            GL(glDisable(GL_STENCIL_TEST));
            GL(glDisable(GL_LIGHTING));
            GL(glDisable(GL_COLOR_MATERIAL));
            GL(glEnable(GL_SCISSOR_TEST));
            GL(glEnableClientState(GL_VERTEX_ARRAY));
            GL(glEnableClientState(GL_TEXTURE_COORD_ARRAY));
            GL(glEnableClientState(GL_COLOR_ARRAY));
            // GL(glDisableClientState(GL_NORMAL_ARRAY));
            GL(glEnable(GL_TEXTURE_2D));
            GL(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
            GL(glShadeModel(GL_SMOOTH));
            GL(glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE));

            // Setup viewport, orthographic projection matrix
            // Our visible imgui space lies from m_drawData->DisplayPos (top left)
            // to m_drawData->DisplayPos+data_data->DisplaySize (bottom right).
            // DisplayPos is (0,0) for single viewport apps.
            GL(glViewport(0, 0, m_fbWidth, m_fbHeight));
            GL(glMatrixMode(GL_PROJECTION));
            GL(glPushMatrix());
            GL(glLoadIdentity());
            GL(glOrtho(m_drawData->DisplayPos.x,
                       m_drawData->DisplayPos.x + m_drawData->DisplaySize.x,
                       m_drawData->DisplayPos.y + m_drawData->DisplaySize.y,
                       m_drawData->DisplayPos.y,
                       -1.0f, +1.0f));
            GL(glMatrixMode(GL_MODELVIEW));
            GL(glPushMatrix());
            GL(glLoadIdentity());
        }

    private:
        void updateTexture(ImTextureData* textureData) const
        {
            if (textureData->Status == ImTextureStatus_WantCreate)
            {
                auto old = cRenderer::getCurrentTexture();

                // Create and upload new texture to graphics system
                IM_ASSERT(textureData->TexID == 0 && textureData->BackendUserData == nullptr);
                IM_ASSERT(textureData->Format == ImTextureFormat_RGBA32);
                auto tex = cRenderer::createTexture();

                // Upload texture to graphics system
                // (Bilinear sampling is required by default.
                // Set 'io.Fonts->Flags |= ImFontAtlasFlags_NoBakedLines'
                // or 'style.AntiAliasedLinesUseTex = false' to allow point/nearest sampling)
                cRenderer::bindTexture(tex);
                GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
                GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
                GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP));
                GL(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP));
                GL(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));
                GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
                auto pixels = textureData->GetPixels();
                GL(glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, textureData->Width, textureData->Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels));

                // Store identifiers
                textureData->SetTexID(tex);
                textureData->SetStatus(ImTextureStatus_OK);

                cRenderer::bindTexture(old);
            }
            else if (textureData->Status == ImTextureStatus_WantUpdates)
            {
                auto old = cRenderer::getCurrentTexture();

                // Update selected blocks. We only ever write to textures regions
                // which have never been used before!
                // This backend choose to use textureData->Updates[] but you can use
                // textureData->UpdateRect to upload a single region.
                cRenderer::bindTexture(textureData->TexID);
                GL(glPixelStorei(GL_UNPACK_ROW_LENGTH, textureData->Width));
                GL(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));
                for (auto& r : textureData->Updates)
                {
                    GL(glTexSubImage2D(GL_TEXTURE_2D, 0, r.x, r.y, r.w, r.h, GL_RGBA, GL_UNSIGNED_BYTE, textureData->GetPixelsAt(r.x, r.y)));
                }
                GL(glPixelStorei(GL_UNPACK_ROW_LENGTH, 0));

                textureData->SetStatus(ImTextureStatus_OK);

                cRenderer::bindTexture(old);
            }
            else if (textureData->Status == ImTextureStatus_WantDestroy)
            {
                cRenderer::deleteTexture(textureData->TexID);

                // Clear identifiers and mark as destroyed
                // (in order to allow e.g. calling InvalidateDeviceObjects while running)
                textureData->SetTexID(ImTextureID_Invalid);
                textureData->SetStatus(ImTextureStatus_Destroyed);
            }
        }

    private:
        const ImDrawData* m_drawData;
        const int m_fbWidth;
        const int m_fbHeight;
    };

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
    // if (glfwGetWindowAttrib(m_window, GLFW_FOCUSED))
    io.MousePos = ImVec2(pos.x, pos.y);
    // io.MousePos = ImVec2(-1, -1);
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

void cGui::init(GLFWwindow* window)
{
    m_window = window;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    auto& io = ImGui::GetIO();

    // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    // io.RenderDrawListsFn = nullptr;
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    io.SetClipboardTextFn = [](void* user_data, const char* text) {
        glfwSetClipboardString(static_cast<GLFWwindow*>(user_data), text);
    };

    io.GetClipboardTextFn = [](void* user_data) {
        return glfwGetClipboardString(static_cast<GLFWwindow*>(user_data));
    };

    io.ClipboardUserData = m_window;

    const ImWchar range[] = { 0x0020, 0xFFFF, 0 };
    io.Fonts->AddFontFromMemoryCompressedTTF(DroidSans_compressed_data, DroidSans_compressed_size, 16, nullptr, range);
    io.BackendFlags |= ImGuiBackendFlags_RendererHasTextures; // We can honor ImGuiPlatformIO::Textures[] requests during render.

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    auto& s = ImGui::GetStyle();
    s.WindowTitleAlign = { 0.5f, 0.5f };
    s.WindowRounding = 5.0f;
    s.WindowPadding = { 3.0f, 3.0f };
}

void cGui::shutdown()
{
    ImGui::DestroyContext();
}

void cGui::beginFrame()
{
    auto& io = ImGui::GetIO();

    // Setup display size (every frame to accommodate for window resizing)
    int w, h;
    int display_w, display_h;
    glfwGetWindowSize(m_window, &w, &h);
    glfwGetFramebufferSize(m_window, &display_w, &display_h);
    io.DisplaySize = ImVec2((float)w, (float)h);
    io.DisplayFramebufferScale = ImVec2(w > 0 ? ((float)display_w / w) : 0, h > 0 ? ((float)display_h / h) : 0);

    // Setup time step
    double current_time = glfwGetTime();
    io.DeltaTime = m_time > 0.0 ? (float)(current_time - m_time) : (float)(1.0f / 60.0f);
    m_time = current_time;

    // Hide OS mouse cursor if ImGui is drawing it
    // glfwSetInputMode(m_window, GLFW_CURSOR, io.MouseDrawCursor ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);

    ImGui::NewFrame();
}

void cGui::endFrame()
{
    ImGui::Render();

    // This is the main rendering function that you have to implement and provide to
    // ImGui (via setting up 'RenderDrawListsFn' in the ImGuiIO structure)
    // If text or lines are blurry when integrating ImGui in your engine:
    // - in your Render function, try translating your projection matrix by (0.5f,0.5f) or (0.375f,0.375f)
    auto drawData = ImGui::GetDrawData();

    // Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
    auto& io = ImGui::GetIO();
    auto width = static_cast<int>(io.DisplaySize.x * io.DisplayFramebufferScale.x);
    auto height = static_cast<int>(io.DisplaySize.y * io.DisplayFramebufferScale.y);
    if (width == 0 || height == 0)
    {
        return;
    }

    RenderState renderState(drawData, width, height);

    renderState.resetState();

    // Will project scissor/clipping rectangles into framebuffer space
    const auto& clipOff = drawData->DisplayPos;         // (0,0) unless using multi-viewports
    const auto& clipScale = drawData->FramebufferScale; // (1,1) unless using retina display which are often (2,2)

    for (const auto drawList : drawData->CmdLists)
    {
        auto vtx_buffer = reinterpret_cast<const char*>(drawList->VtxBuffer.Data);
        GL(glVertexPointer(2, GL_FLOAT, sizeof(ImDrawVert),
                           reinterpret_cast<const GLvoid*>(vtx_buffer + offsetof(ImDrawVert, pos))));
        GL(glTexCoordPointer(2, GL_FLOAT, sizeof(ImDrawVert),
                             reinterpret_cast<const GLvoid*>(vtx_buffer + offsetof(ImDrawVert, uv))));
        GL(glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert),
                          reinterpret_cast<const GLvoid*>(vtx_buffer + offsetof(ImDrawVert, col))));

        for (int idx = 0; idx < drawList->CmdBuffer.Size; idx++)
        {
            const auto pcmd = &drawList->CmdBuffer[idx];
            if (pcmd->UserCallback)
            {
                // User callback, registered via ImDrawList::AddCallback()
                // (ImDrawCallback_ResetRenderState is a special callback value
                // used by the user to request the renderer to reset render state.)
                if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
                {
                    renderState.resetState();
                }
                else
                {
                    pcmd->UserCallback(drawList, pcmd);
                }
            }
            else
            {
                // Project scissor/clipping rectangles into framebuffer space
                ImVec2 clipMin{ (pcmd->ClipRect.x - clipOff.x) * clipScale.x,
                                (pcmd->ClipRect.y - clipOff.y) * clipScale.y };
                ImVec2 clipMax{ (pcmd->ClipRect.z - clipOff.x) * clipScale.x,
                                (pcmd->ClipRect.w - clipOff.y) * clipScale.y };
                if (clipMax.x <= clipMin.x || clipMax.y <= clipMin.y)
                {
                    continue;
                }

                // Apply scissor/clipping rectangle (Y is inverted in OpenGL)
                GL(glScissor(static_cast<int>(clipMin.x),
                             static_cast<int>((float)height - clipMax.y),
                             static_cast<int>(clipMax.x - clipMin.x),
                             static_cast<int>(clipMax.y - clipMin.y)));

                // Bind texture, Draw
                cRenderer::bindTexture(pcmd->GetTexID());
                auto type = sizeof(ImDrawIdx) == 2
                    ? GL_UNSIGNED_SHORT
                    : GL_UNSIGNED_INT;
                auto idx_buffer = drawList->IdxBuffer.Data;
                GL(glDrawElements(GL_TRIANGLES, pcmd->ElemCount, type, idx_buffer + pcmd->IdxOffset));
            }
        }
    }
}

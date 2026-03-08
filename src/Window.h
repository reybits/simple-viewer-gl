/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Types/Types.h"
#include "Types/Vector.h"

#include <functional>

struct GLFWwindow;
struct sConfig;

struct sWindowEvents
{
    std::function<void(const Vectori& winSize)> onWindowResize;
    std::function<void(const Vectori& fbSize)> onFramebufferResize;
    std::function<void(const Vectori& pos)> onWindowPosition;
    std::function<void()> onWindowRefresh;
    std::function<void(int key, int scancode, int action, int mods)> onKeyEvent;
    std::function<void(uint32_t codepoint)> onCharEvent;
    std::function<void(int button, int action, int mods)> onMouseButton;
    std::function<void(const Vectorf& pos)> onMouseMove;
    std::function<void(const Vectorf& offset)> onMouseScroll;
    std::function<void(const StringsList& paths)> onFileDrop;
};

class cWindow final
{
public:
    cWindow() = default;
    ~cWindow();

    bool init(const sConfig& config);
    void shutdown();

    void setEventHandler(sWindowEvents* handler);

    // Queries
    Vectori getWindowSize() const;
    Vectori getFramebufferSize() const;
    Vectori getScreenSize() const;
    Vectorf getPixelRatio() const;
    Vectorf getCursorPos() const;

    // Manipulation
    void setSize(const Vectori& size);
    void setPosition(const Vectori& pos);
    void setTitle(const char* title);
    void setCursorVisible(bool visible);
    void setClipboardText(const char* text);
    const char* getClipboardText() const;
    void requestClose();
    bool shouldClose() const;

    // Fullscreen toggle — handles window recreation + GL re-init
    void toggleFullscreen(const sConfig& config);
    bool isWindowed() const
    {
        return m_windowed;
    }

    // Main loop operations
    void pollEvents();
    void swapBuffers();

    // For ImGui and renderer init
    GLFWwindow* getNativeHandle() const
    {
        return m_window;
    }

private:
    void setupCallbacks();
    GLFWwindow* createWindowedWindow(GLFWwindow* parent, const sConfig& config);
    GLFWwindow* createFullscreenWindow(GLFWwindow* parent, const sConfig& config);
    void setHints(const sConfig& config);

private:
    GLFWwindow* m_window = nullptr;
    sWindowEvents* m_handler = nullptr;
    bool m_windowed = true;

    // macOS Mojave workaround
    int m_macOSHackCount = 0;
};

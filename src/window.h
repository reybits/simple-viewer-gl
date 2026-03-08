/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include "types/types.h"
#include "types/vector.h"

struct GLFWwindow;
struct sConfig;

class iWindowEvents
{
public:
    virtual ~iWindowEvents() = default;

    virtual void onWindowResize(const Vectori& winSize) = 0;
    virtual void onFramebufferResize(const Vectori& fbSize) = 0;
    virtual void onWindowPosition(const Vectori& pos) = 0;
    virtual void onWindowRefresh() = 0;
    virtual void onKeyEvent(int key, int scancode, int action, int mods) = 0;
    virtual void onCharEvent(uint32_t codepoint) = 0;
    virtual void onMouseButton(int button, int action, int mods) = 0;
    virtual void onMouseMove(const Vectorf& pos) = 0;
    virtual void onMouseScroll(const Vectorf& offset) = 0;
    virtual void onFileDrop(const StringsList& paths) = 0;
};

class cWindow final
{
public:
    cWindow() = default;
    ~cWindow();

    bool init(const sConfig& config);
    void shutdown();

    void setEventHandler(iWindowEvents* handler);

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
    bool isWindowed() const { return m_windowed; }

    // Main loop operations
    void pollEvents();
    void swapBuffers();

    // For ImGui and renderer init
    GLFWwindow* getNativeHandle() const { return m_window; }

private:
    void setupCallbacks();
    GLFWwindow* createWindowedWindow(GLFWwindow* parent, const sConfig& config);
    GLFWwindow* createFullscreenWindow(GLFWwindow* parent, const sConfig& config);
    void setHints(const sConfig& config);

private:
    GLFWwindow* m_window = nullptr;
    iWindowEvents* m_handler = nullptr;
    bool m_windowed = true;

    // macOS Mojave workaround
    int m_macOSHackCount = 0;
};

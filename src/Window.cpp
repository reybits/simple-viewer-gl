/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "Window.h"
#include "Common/Config.h"
#include "Common/Helpers.h"
#include "Log/Log.h"
#include "Version.h"

#include <algorithm>
#include <glad/glad.h>

#include <GLFW/glfw3.h>

namespace
{
    cWindow* Instance = nullptr;

    template <typename Fn, typename... Args>
    void dispatch(sWindowEvents* handler, Fn& fn, Args&&... args)
    {
        if (handler && fn)
        {
            fn(std::forward<Args>(args)...);
        }
    }

} // namespace

cWindow::~cWindow()
{
    shutdown();
}

void cWindow::setHints(const sConfig& config)
{
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    auto className = config.className.c_str();
    glfwWindowHintString(GLFW_X11_CLASS_NAME, className);

    if (helpers::getPlatform() == helpers::Platform::Wayland)
    {
#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 4
        glfwWindowHintString(GLFW_WAYLAND_APP_ID, className);
        glfwWindowHint(GLFW_ANY_POSITION, true);
#endif
    }
}

GLFWwindow* cWindow::createWindowedWindow(GLFWwindow* parent, const sConfig& config)
{
    setHints(config);
    auto width = std::max(config.windowSize.w, DefaultWindowSize.w);
    auto height = std::max(config.windowSize.h, DefaultWindowSize.h);
    return glfwCreateWindow(width, height, version::getTitle(), nullptr, parent);
}

GLFWmonitor* cWindow::getCurrentMonitor() const
{
    if (m_window == nullptr)
    {
        return glfwGetPrimaryMonitor();
    }

    int wx = 0, wy = 0, ww = 0, wh = 0;
    glfwGetWindowPos(m_window, &wx, &wy);
    glfwGetWindowSize(m_window, &ww, &wh);

    const auto centerX = wx + ww / 2;
    const auto centerY = wy + wh / 2;

    int monitorCount = 0;
    auto monitors = glfwGetMonitors(&monitorCount);
    if (monitors != nullptr)
    {
        for (int i = 0; i < monitorCount; i++)
        {
            int mx = 0, my = 0, mw = 0, mh = 0;
            glfwGetMonitorWorkarea(monitors[i], &mx, &my, &mw, &mh);
            if (centerX >= mx && centerX < mx + mw && centerY >= my && centerY < my + mh)
            {
                return monitors[i];
            }
        }
    }

    return glfwGetPrimaryMonitor();
}

GLFWwindow* cWindow::createFullscreenWindow(GLFWwindow* parent, const sConfig& config)
{
    setHints(config);
    auto monitor = getCurrentMonitor();
    if (monitor == nullptr)
    {
        cLog::Error("No monitor found, falling back to windowed mode.");
        return createWindowedWindow(parent, config);
    }
    auto mode = glfwGetVideoMode(monitor);
    if (mode == nullptr)
    {
        cLog::Error("Can't get video mode, falling back to windowed mode.");
        return createWindowedWindow(parent, config);
    }
    return glfwCreateWindow(mode->width, mode->height, version::getTitle(), monitor, parent);
}

void cWindow::setupCallbacks()
{
    glfwSetWindowSizeCallback(m_window, [](GLFWwindow*, int w, int h) {
        if (Instance == nullptr)
            return;
        dispatch(Instance->m_handler, Instance->m_handler->onWindowResize, Vectori{ w, h });
    });
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow*, int w, int h) {
        if (Instance == nullptr)
            return;
        dispatch(Instance->m_handler, Instance->m_handler->onFramebufferResize, Vectori{ w, h });
    });
    glfwSetWindowPosCallback(m_window, [](GLFWwindow*, int x, int y) {
        if (Instance == nullptr)
            return;
        dispatch(Instance->m_handler, Instance->m_handler->onWindowPosition, Vectori{ x, y });
    });
    glfwSetWindowRefreshCallback(m_window, [](GLFWwindow*) {
        if (Instance == nullptr)
            return;
        dispatch(Instance->m_handler, Instance->m_handler->onWindowRefresh);
    });
    glfwSetKeyCallback(m_window, [](GLFWwindow*, int key, int scancode, int action, int mods) {
        if (Instance == nullptr)
            return;
        dispatch(Instance->m_handler, Instance->m_handler->onKeyEvent, key, scancode, action, mods);
    });
    glfwSetCharCallback(m_window, [](GLFWwindow*, unsigned int c) {
        if (Instance == nullptr)
            return;
        dispatch(Instance->m_handler, Instance->m_handler->onCharEvent, c);
    });
    glfwSetInputMode(m_window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);
    glfwSetMouseButtonCallback(m_window, [](GLFWwindow*, int button, int action, int mods) {
        if (Instance == nullptr)
            return;
        dispatch(Instance->m_handler, Instance->m_handler->onMouseButton, button, action, mods);
    });
    glfwSetCursorPosCallback(m_window, [](GLFWwindow*, double x, double y) {
        if (Instance == nullptr)
            return;
        dispatch(Instance->m_handler, Instance->m_handler->onMouseMove, Vectorf{ static_cast<float>(x), static_cast<float>(y) });
    });
    glfwSetScrollCallback(m_window, [](GLFWwindow*, double x, double y) {
        if (Instance == nullptr)
            return;
        dispatch(Instance->m_handler, Instance->m_handler->onMouseScroll, Vectorf{ static_cast<float>(x), static_cast<float>(y) });
    });
#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 1
    glfwSetDropCallback(m_window, [](GLFWwindow*, int count, const char** paths) {
        if (Instance == nullptr)
            return;
        auto handler = Instance->m_handler;
        if (handler && handler->onFileDrop)
        {
            StringsList list;
            for (int i = 0; i < count; i++)
            {
                list.push_back(paths[i]);
            }
            handler->onFileDrop(list);
        }
    });
#endif
}

bool cWindow::init(const sConfig& config)
{
    Instance = this;

    glfwSetErrorCallback([](int error_code, const char* description) {
        cLog::Error("GLFW error ({}): '{}'.", error_code, description);
    });

    if (!glfwInit())
    {
        cLog::Error("Can't initialize GLFW.");
        return false;
    }

    m_windowed = !config.fullScreen;

    if (config.fullScreen)
    {
        m_window = createFullscreenWindow(nullptr, config);
    }
    else
    {
        m_window = createWindowedWindow(nullptr, config);
    }

    if (m_window == nullptr)
    {
        cLog::Error("Can't create window.");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(m_window);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        cLog::Error("Can't initialize GLAD.");
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        glfwTerminate();
        return false;
    }

    glfwSwapInterval(1);
    setupCallbacks();

    return true;
}

void cWindow::shutdown()
{
    if (m_window != nullptr)
    {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
    Instance = nullptr;
}

void cWindow::setEventHandler(sWindowEvents* handler)
{
    m_handler = handler;
}

Vectori cWindow::getWindowSize() const
{
    Vectori size;
    if (m_window != nullptr)
    {
        glfwGetWindowSize(m_window, &size.x, &size.y);
    }
    return size;
}

Vectori cWindow::getFramebufferSize() const
{
    Vectori size;
    if (m_window != nullptr)
    {
        glfwGetFramebufferSize(m_window, &size.x, &size.y);
    }
    return size;
}

Vectori cWindow::getScreenSize() const
{
    auto monitor = getCurrentMonitor();
    if (monitor != nullptr)
    {
        int wx = 0, wy = 0, ww = 0, wh = 0;
        glfwGetMonitorWorkarea(monitor, &wx, &wy, &ww, &wh);
        if (ww > 0 && wh > 0)
        {
            // Subtract window frame (title bar, etc.) to get max content area.
            if (m_window != nullptr)
            {
                int top = 0, left = 0, right = 0, bottom = 0;
                glfwGetWindowFrameSize(m_window, &left, &top, &right, &bottom);
                ww -= left + right;
                wh -= top + bottom;
            }
            return { ww, wh };
        }

        const auto* mode = glfwGetVideoMode(monitor);
        if (mode != nullptr)
        {
            return { mode->width, mode->height };
        }
    }
    return { 1920, 1080 };
}

Vectorf cWindow::getPixelRatio() const
{
    auto win = getWindowSize();
    auto fb = getFramebufferSize();
    return {
        win.x > 0 ? static_cast<float>(fb.x) / win.x : 1.0f,
        win.y > 0 ? static_cast<float>(fb.y) / win.y : 1.0f
    };
}

Vectorf cWindow::getCursorPos() const
{
    double x = 0.0, y = 0.0;
    if (m_window != nullptr)
    {
        glfwGetCursorPos(m_window, &x, &y);
    }
    return { static_cast<float>(x), static_cast<float>(y) };
}

void cWindow::setSize(const Vectori& size)
{
    if (m_window != nullptr)
    {
        glfwSetWindowSize(m_window, size.x, size.y);
    }
}

void cWindow::setPosition(const Vectori& pos)
{
    if (m_window != nullptr)
    {
        glfwSetWindowPos(m_window, pos.x, pos.y);
    }
}

void cWindow::setTitle(const char* title)
{
    if (m_window != nullptr)
    {
        glfwSetWindowTitle(m_window, title);
    }
}

void cWindow::setCursorVisible(bool visible)
{
    if (m_window != nullptr)
    {
        glfwSetInputMode(m_window, GLFW_CURSOR, visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }
}

void cWindow::setClipboardText(const char* text)
{
    if (m_window != nullptr)
    {
        glfwSetClipboardString(m_window, text);
    }
}

const char* cWindow::getClipboardText() const
{
    if (m_window != nullptr)
    {
        return glfwGetClipboardString(m_window);
    }
    return nullptr;
}

void cWindow::requestClose()
{
    if (m_window != nullptr)
    {
        glfwSetWindowShouldClose(m_window, 1);
    }
}

bool cWindow::shouldClose() const
{
    return m_window == nullptr || glfwWindowShouldClose(m_window);
}

void cWindow::toggleFullscreen(const sConfig& config)
{
    const bool wasWindowed = m_windowed;
    GLFWwindow* newWindow = wasWindowed
        ? createFullscreenWindow(m_window, config)
        : createWindowedWindow(m_window, config);

    if (newWindow == nullptr)
    {
        cLog::Error("Can't create new window for fullscreen toggle.");
        return;
    }

    glfwMakeContextCurrent(newWindow);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        cLog::Error("Can't re-initialize GLAD, keeping old window.");
        glfwDestroyWindow(newWindow);
        glfwMakeContextCurrent(m_window);
        return;
    }

    glfwSwapInterval(1);

    glfwDestroyWindow(m_window);
    m_window = newWindow;
    m_windowed = !wasWindowed;

    setupCallbacks();
}

void cWindow::pollEvents()
{
    glfwPollEvents();
}

void cWindow::swapBuffers()
{
    if (m_window != nullptr)
    {
        glfwSwapBuffers(m_window);
    }
}

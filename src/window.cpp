/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "window.h"
#include "common/config.h"
#include "common/helpers.h"
#include "log/Log.h"
#include "version.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

namespace
{
    cWindow* Instance = nullptr;

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

GLFWwindow* cWindow::createFullscreenWindow(GLFWwindow* parent, const sConfig& config)
{
    setHints(config);
    auto monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    return glfwCreateWindow(mode->width, mode->height, version::getTitle(), monitor, parent);
}

void cWindow::setupCallbacks()
{
    glfwSetWindowSizeCallback(m_window, [](GLFWwindow*, int w, int h) {
        if (Instance && Instance->m_handler)
        {
            Instance->m_handler->onWindowResize({ w, h });
        }
    });
    glfwSetFramebufferSizeCallback(m_window, [](GLFWwindow*, int w, int h) {
        if (Instance && Instance->m_handler)
        {
            Instance->m_handler->onFramebufferResize({ w, h });
        }
    });
    glfwSetWindowPosCallback(m_window, [](GLFWwindow*, int x, int y) {
        if (Instance && Instance->m_handler)
        {
            Instance->m_handler->onWindowPosition({ x, y });
        }
    });
    glfwSetWindowRefreshCallback(m_window, [](GLFWwindow*) {
        if (Instance && Instance->m_handler)
        {
            Instance->m_handler->onWindowRefresh();
        }
    });
    glfwSetKeyCallback(m_window, [](GLFWwindow*, int key, int scancode, int action, int mods) {
        if (Instance && Instance->m_handler)
        {
            Instance->m_handler->onKeyEvent(key, scancode, action, mods);
        }
    });
    glfwSetCharCallback(m_window, [](GLFWwindow*, unsigned int c) {
        if (Instance && Instance->m_handler)
        {
            Instance->m_handler->onCharEvent(c);
        }
    });
    glfwSetInputMode(m_window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);
    glfwSetMouseButtonCallback(m_window, [](GLFWwindow*, int button, int action, int mods) {
        if (Instance && Instance->m_handler)
        {
            Instance->m_handler->onMouseButton(button, action, mods);
        }
    });
    glfwSetCursorPosCallback(m_window, [](GLFWwindow*, double x, double y) {
        if (Instance && Instance->m_handler)
        {
            Instance->m_handler->onMouseMove({ static_cast<float>(x), static_cast<float>(y) });
        }
    });
    glfwSetScrollCallback(m_window, [](GLFWwindow*, double x, double y) {
        if (Instance && Instance->m_handler)
        {
            Instance->m_handler->onMouseScroll({ static_cast<float>(x), static_cast<float>(y) });
        }
    });
#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 1
    glfwSetDropCallback(m_window, [](GLFWwindow*, int count, const char** paths) {
        if (Instance && Instance->m_handler)
        {
            StringsList list;
            for (int i = 0; i < count; i++)
            {
                list.push_back(paths[i]);
            }
            Instance->m_handler->onFileDrop(list);
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
        cLog::Error("Failed to initialize GLAD.");
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
    auto monitor = glfwGetPrimaryMonitor();
    if (monitor != nullptr)
    {
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
    GLFWwindow* newWindow = nullptr;

    if (m_windowed)
    {
        newWindow = createFullscreenWindow(m_window, config);
        m_windowed = false;
    }
    else
    {
        newWindow = createWindowedWindow(m_window, config);
        m_windowed = true;
    }

    glfwMakeContextCurrent(newWindow);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        cLog::Error("Failed to re-initialize GLAD.");
    }

    glfwSwapInterval(1);

    glfwDestroyWindow(m_window);
    m_window = newWindow;

    setupCallbacks();
    m_macOSHackCount = 0;
}

void cWindow::pollEvents()
{
    // macOS Mojave workaround (GLFW #1334)
#if defined(__APPLE__)
    if (m_macOSHackCount < 2)
    {
        m_macOSHackCount++;
        int x, y;
        glfwGetWindowPos(m_window, &x, &y);
        if (m_macOSHackCount == 1)
        {
            glfwSetWindowPos(m_window, x + 1, y);
        }
        else
        {
            glfwSetWindowPos(m_window, x - 1, y);
        }
    }
#endif

    glfwPollEvents();
}

void cWindow::swapBuffers()
{
    if (m_window != nullptr)
    {
        glfwSwapBuffers(m_window);
    }
}

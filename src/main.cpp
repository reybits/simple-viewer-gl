/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "common/config.h"
#include "common/helpers.h"
#include "types/types.h"
#include "version.h"
#include "viewer.h"

#include <GLFW/glfw3.h>

#include <clocale>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace
{
    cViewer* m_viewer = nullptr;

    void showVersion()
    {
        printf("%s %d.%d.%d\n\n",
               version::getTitle(),
               version::getMajor(),
               version::getMinor(),
               version::getRelease());
        printf("Copyright © 2008-2024 Andrey A. Ugolnik. All Rights Reserved.\n");
        printf("https://www.ugolnik.info\n");
        printf("andrey@ugolnik.info\n");
    }

    const char* getValue(bool enabled)
    {
        return enabled ? "enabled" : "disabled";
    }

    void showHelp(const char* name, const sConfig& config)
    {
        const char* p = strrchr(name, '/');

        printf("\nUsage:\n");
        printf("  %s [OPTION]... FILE\n", (p != nullptr ? p + 1 : name));
        printf("  -h, --help     show this help;\n");
        printf("  -v, --version  show viewer version;\n");
        printf("  --class VALUE  class name (default: %s);\n", config.className.c_str());
        printf("  -s             enable scale to window (default: %s);\n", getValue(config.fitImage));
        printf("  -cw            center window (default: %s);\n", getValue(config.centerWindow));
        printf("  -a             do not filter by file extension;\n");
        printf("  -c             user defined background color (default #%.2x%.2x%.2x);\n", (uint32_t)config.bgColor.r, (uint32_t)config.bgColor.g, (uint32_t)config.bgColor.b);
        printf("  -i             disable on-screen info (default: %s);\n", getValue(!config.hideInfobar));
        printf("  -p             show pixel info (pixel color and coordinates, default: %s);\n", getValue(config.showPixelInfo));
        printf("  -e             show exif info (default: %s);\n", getValue(config.showExif));
        printf("  -b             show border around image (default: %s);\n", getValue(config.showImageBorder));
        printf("  -g             show image grid (default: %s);\n", getValue(config.showImageGrid));
        printf("  -f             start in fullscreen mode (default: %s);\n", getValue(config.fullScreen));
        printf("  -r             recursive directory scan (default: %s);\n", getValue(config.recursiveScan));
        printf("  -wz            enable wheel zoom (default: %s);\n", getValue(config.wheelZoom));
        printf("  -mipmap VALUE  min texture size for mipmap creation (default: %u px);\n", config.mipmapTextureSize);
        printf("  -svg SIZE      min SVG size (default: %g px);\n", config.minSvgSize);
        printf("  -C RRGGBB      background color in hex format (default: %.2X%.2X%.2X);\n",
               (uint32_t)config.bgColor.r,
               (uint32_t)config.bgColor.g,
               (uint32_t)config.bgColor.b);
        printf("  --             read null terminated files list from stdin.\n");
        printf("                 Usage:\n");
        printf("                   find /path -name *.psd -print0 | sviewgl --\n");

        printf("\nAvailable keys:\n");
        printf("  <esc> or <q>  exit;\n");
        printf("  <space>       next image;\n");
        printf("  <backspace>   previous image;\n");
        printf("  <+> / <->     scale image;\n");
        printf("  <1>...<0>     set scale from 100%% to 1000%%;\n");
        printf("  <pgdn>        next image in multi-page image;\n");
        printf("  <pgup>        previous image in multi-page image;\n");
        printf("  <enter>       switch fullscreen / windowed mode;\n");
        printf("  <del>         toggle deletion mark;\n");
        printf("  <ctrl>+<del>  delete marked images from disk;\n");
        printf("  <s>           fit image to window;\n");
        printf("  <r>           rotate clockwise;\n");
        printf("  <shift>+<r>   rotate counterclockwise;\n");
        printf("  <c>           change background index;\n");
        printf("  <i>           hide / show on-screen info;\n");
        printf("  <e>           hide / show exif popup;\n");
        printf("  <p>           hide / show pixel info;\n");
        printf("  <b>           hide / show border around image;\n");
        printf("  <g>           hide / show image grid;\n");
        printf("\n");
    }

    void callbackResize(GLFWwindow* /*window*/, int width, int height)
    {
        m_viewer->onWindowResize({ width, height });
    }

    void callbackFramebufferResize(GLFWwindow* /*window*/, int width, int height)
    {
        m_viewer->onFramebufferResize({ width, height });
    }

    void callbackPosition(GLFWwindow* /*window*/, int x, int y)
    {
        m_viewer->onPosition({ x, y });
    }

    void callbackRedraw(GLFWwindow* /*window*/)
    {
        m_viewer->onRender();
    }

    void callbackKey(GLFWwindow* /*window*/, int key, int scancode, int action, int mods)
    {
        m_viewer->onKey(key, scancode, action, mods);
    }

    void callbackChar(GLFWwindow* /*window*/, unsigned int c)
    {
        m_viewer->onChar(c);
    }

    void callbackMouseButtons(GLFWwindow* /*window*/, int button, int action, int mods)
    {
        m_viewer->onMouseButtons(button, action, mods);
    }

    void callbackMousePosition(GLFWwindow* /*window*/, double x, double y)
    {
        m_viewer->onMouse({ (float)x, (float)y });
    }

    void callbackCursorEnter(GLFWwindow* /*window*/, int entered)
    {
        m_viewer->onCursorEnter(entered != 0);
    }

    void callbackMouseScroll(GLFWwindow* /*window*/, double x, double y)
    {
        m_viewer->onMouseScroll({ (float)x, (float)y });
    }

#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 1
    void callbackDrop(GLFWwindow* /*window*/, int count, const char** paths)
    {
        StringsList imagesList;
        for (int i = 0; i < count; i++)
        {
            imagesList.push_back(paths[i]);
        }
        m_viewer->addPaths(imagesList);
    }
#endif

    void callbackError(int e, const char* error)
    {
        ::printf("(EE) GLFW error (%d) '%s'\n", e, error);
    }

    void setHints(const sConfig& config)
    {
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

    void setContext(GLFWwindow* window)
    {
        glfwSetErrorCallback(callbackError);

        glfwMakeContextCurrent(window);
        glfwSwapInterval(1);

        glfwSetWindowSizeCallback(window, callbackResize);
        glfwSetFramebufferSizeCallback(window, callbackFramebufferResize);
        glfwSetWindowPosCallback(window, callbackPosition);

        glfwSetWindowRefreshCallback(window, callbackRedraw);

        glfwSetKeyCallback(window, callbackKey);
        glfwSetCharCallback(window, callbackChar);

        glfwSetInputMode(window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);
        glfwSetMouseButtonCallback(window, callbackMouseButtons);
        glfwSetCursorPosCallback(window, callbackMousePosition);
        glfwSetScrollCallback(window, callbackMouseScroll);
        // INFO: Disabled due to a bug in GLFW.
        // Check the actual cursor position instead.
        // glfwSetCursorEnterCallback(window, callbackCursorEnter);

#if GLFW_VERSION_MAJOR >= 3 && GLFW_VERSION_MINOR >= 1
        glfwSetDropCallback(window, callbackDrop);
#endif
    }

    GLFWwindow* createWindowedWindow(GLFWwindow* parent, const sConfig& config)
    {
        setHints(config);

        auto width = std::max(config.windowSize.w, DefaultWindowSize.w);
        auto height = std::max(config.windowSize.h, DefaultWindowSize.h);

        auto newWindow = glfwCreateWindow(width, height, version::getTitle(), nullptr, parent);
        return newWindow;
    }

    GLFWwindow* createFullscreenWindow(GLFWwindow* parent, const sConfig& config)
    {
        setHints(config);

        auto monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);
        auto newWindow = glfwCreateWindow(mode->width, mode->height, version::getTitle(), monitor, parent);
        return newWindow;
    }

    //
    // macOS Mojave / Xcode 10 specific bug workaround
    // https://github.com/glfw/glfw/issues/1334
    //
    static int macOSHackCount = 0;

    void macOSMojaveUglyHack(GLFWwindow* window)
    {
#if defined(__APPLE__)
        if (macOSHackCount < 2)
        {
            macOSHackCount++;
            if (macOSHackCount == 1)
            {
                int x, y;
                glfwGetWindowPos(window, &x, &y);
                glfwSetWindowPos(window, ++x, y);
            }
            else if (macOSHackCount == 2)
            {
                int x, y;
                glfwGetWindowPos(window, &x, &y);
                glfwSetWindowPos(window, --x, y);
            }
        }
#endif
        (void)window;
    }

    StringsList ImagesList;

} // namespace

extern "C" {
void AddFile(const char* filename)
{
    if (m_viewer == nullptr)
    {
        if (filename != nullptr)
        {
            ImagesList.push_back(filename);
        }
    }
    else
    {
        StringsList imagesList;
        imagesList.push_back(filename);
        m_viewer->addPaths(imagesList);
    }
}

} // extern "C"

int main(int argc, char* argv[])
{
    ::setlocale(LC_ALL, "");

    sConfig config;

    for (int i = 1; i < argc; i++)
    {
        if (!::strncmp(argv[i], "-h", 2) || !::strncmp(argv[i], "--help", 6))
        {
            showVersion();
            showHelp(argv[0], config);
            return 0;
        }
        else if (!::strncmp(argv[i], "-v", 2) || !::strncmp(argv[i], "--version", 9))
        {
            showVersion();
            return 0;
        }
    }

    cConfig fileConfig;
    fileConfig.read(config);

    for (int i = 1; i < argc; i++)
    {
        if (::strncmp(argv[i], "--debug", 7) == 0)
        {
            config.debug = true;
        }
        if (::strncmp(argv[i], "--class", 7) == 0)
        {
            if (i + 1 < argc)
            {
                config.className = argv[++i];
            }
        }
        else if (::strncmp(argv[i], "-mipmap", 7) == 0)
        {
            if (i + 1 < argc)
            {
                config.mipmapTextureSize = (uint32_t)::atoi(argv[++i]);
            }
        }
        else if (::strncmp(argv[i], "-svg", 4) == 0)
        {
            if (i + 1 < argc)
            {
                config.minSvgSize = (float)::atof(argv[++i]);
            }
        }
        else if (::strncmp(argv[i], "-cw", 3) == 0)
        {
            config.centerWindow = true;
        }
        else if (::strncmp(argv[i], "-wz", 3) == 0)
        {
            config.wheelZoom = true;
        }
        else if (::strncmp(argv[i], "-i", 2) == 0)
        {
            config.hideInfobar = true;
        }
        else if (::strncmp(argv[i], "-p", 2) == 0)
        {
            config.showPixelInfo = true;
        }
        else if (::strncmp(argv[i], "-e", 2) == 0)
        {
            config.showExif = true;
        }
        else if (::strncmp(argv[i], "-f", 2) == 0)
        {
            config.fullScreen = true;
        }
        else if (::strncmp(argv[i], "-c", 2) == 0)
        {
            config.backgroundIndex = 1;
        }
        else if (::strncmp(argv[i], "-s", 2) == 0)
        {
            config.fitImage = true;
        }
        else if (::strncmp(argv[i], "-b", 2) == 0)
        {
            config.showImageBorder = true;
        }
        else if (::strncmp(argv[i], "-g", 2) == 0)
        {
            config.showImageGrid = true;
        }
        else if (::strncmp(argv[i], "-r", 2) == 0)
        {
            config.recursiveScan = true;
        }
        else if (::strncmp(argv[i], "-a", 2) == 0)
        {
            config.skipFilter = true;
        }
        else if (::strncmp(argv[i], "-C", 2) == 0)
        {
            uint32_t r, g, b;
            if (3 == sscanf(argv[i + 1], "%2x%2x%2x", &r, &g, &b))
            {
                config.bgColor = { (uint8_t)r, (uint8_t)g, (uint8_t)b, (uint8_t)255 };
                i++;
            }
        }
        else if (strcmp(argv[i], "--") == 0)
        {
            std::string path;
            while (1)
            {
                auto c = ::getc(stdin);
                if (c == EOF)
                {
                    break;
                }
                else if (c == 0)
                {
                    ImagesList.push_back(path);
                    path.clear();
                }
                else
                {
                    path += c;
                }
            }
        }
        else
        {
            auto path = argv[i];
            if (path[0] != '-')
            {
                ImagesList.push_back(path);
            }
        }
    }

    int result = 0;
    glfwSetErrorCallback([](int error_code, const char* description) {
        ::printf("(EE) %d: '%s'\n", error_code, description);
    });

    if (glfwInit())
    {
        cViewer viewer(config);
        m_viewer = &viewer;

        viewer.addPaths(ImagesList);
        ImagesList.clear();

        GLFWwindow* window = nullptr;
        if (config.fullScreen)
        {
            window = createFullscreenWindow(nullptr, config);
            viewer.setWindowed(false);
        }
        else
        {
            window = createWindowedWindow(nullptr, config);
            viewer.setWindowed(true);
        }

        bool updateSizePos = !(config.fullScreen || config.centerWindow);

        if (window != nullptr)
        {
            setContext(window);
            viewer.setWindow(window);

            while (glfwWindowShouldClose(window) == false)
            {
                macOSMojaveUglyHack(window);

                if (viewer.isWindowModeRequested())
                {
                    GLFWwindow* newWindow = nullptr;

                    const bool windowed = viewer.isWindowed();
                    if (windowed == false)
                    {
                        updateSizePos = true;

                        newWindow = createWindowedWindow(window, config);
                        viewer.setWindowed(true);
                    }
                    else
                    {
                        newWindow = createFullscreenWindow(window, config);
                        viewer.setWindowed(false);
                    }

                    setContext(newWindow);
                    viewer.setWindow(newWindow);

                    glfwDestroyWindow(window);
                    window = newWindow;

                    macOSHackCount = 0;
                }
                else if (updateSizePos)
                {
                    updateSizePos = false;
                    viewer.setWindowed(true);

                    if (helpers::getPlatform() != helpers::Platform::Wayland)
                    {
                        glfwSetWindowSize(window, config.windowSize.x, config.windowSize.y);
                        glfwSetWindowPos(window, config.windowPos.x, config.windowPos.y);
                    }
                }

                const float timeStart = glfwGetTime();
                static float timeEnd = timeStart;

                viewer.onRender();
                viewer.onUpdate();

                glfwPollEvents();

                if (viewer.isUploading() == false)
                {
                    const float delta = timeEnd - timeStart;

                    const float disiredFps = 1.0f / 60.0f;
                    const float time_rest = disiredFps - delta;
                    if (time_rest > 0.0f)
                    {
                        usleep(time_rest * 1000000);
                    }
                }

                timeEnd = glfwGetTime();
            }

            fileConfig.write(config);
        }
        else
        {
            ::printf("(EE) Can't create window.\n");
            result = -1;
        }

        glfwTerminate();
    }
    else
    {
        ::printf("(EE) Can't initialize GLFW.\n");
        result = -1;
    }

    return result;
}

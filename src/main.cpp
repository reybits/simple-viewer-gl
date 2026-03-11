/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "Common/Config.h"
#include "Common/Helpers.h"
#include "Common/Timing.h"
#include "Log/Log.h"
#include "Types/Types.h"
#include "Version.h"
#include "Viewer.h"
#include "Window.h"

#include <clocale>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

namespace
{
    cViewer* Viewer = nullptr;
    StringsList ImagesList;

    void showVersion()
    {
        cLog::Info("{} {}.{}.{}",
                   version::getTitle(),
                   version::getMajor(),
                   version::getMinor(),
                   version::getRelease());
        cLog::Info("");
        cLog::Info("Copyright © 2008-2026 Andrey A. Ugolnik. All Rights Reserved.");
        cLog::Info("https://github.com/reybits");
        cLog::Info("and@reybits.dev");
    }

    const char* getValue(bool enabled)
    {
        return enabled ? "enabled" : "disabled";
    }

    void showHelp(const char* name, const sConfig& config)
    {
        const char* p = strrchr(name, '/');

        cLog::Info("");
        cLog::Info("Usage:");
        cLog::Info("  {} [OPTION]... FILE", (p != nullptr ? p + 1 : name));
        cLog::Info("");
        cLog::Info("Options:");
        cLog::Info("  -h, --help     show this help");
        cLog::Info("  -v, --version  show viewer version");
        cLog::Info("  --class VALUE  window class name (default: {})", config.className);
        cLog::Info("  -s             fit image to window (default: {})", getValue(config.fitImage));
        cLog::Info("  -cw            center window (default: {})", getValue(config.centerWindow));
        cLog::Info("  -a             do not filter by file extension");
        cLog::Info("  -c             use custom background color");
        cLog::Info("  -C RRGGBB      background color in hex (default: {:02X}{:02X}{:02X})",
                   static_cast<uint32_t>(config.bgColor.r),
                   static_cast<uint32_t>(config.bgColor.g),
                   static_cast<uint32_t>(config.bgColor.b));
        cLog::Info("  -i             disable on-screen info (default: {})", getValue(!config.hideInfobar));
        cLog::Info("  -p             show pixel info (default: {})", getValue(config.showPixelInfo));
        cLog::Info("  -e             show exif info (default: {})", getValue(config.showExif));
        cLog::Info("  -b             show border around image (default: {})", getValue(config.showImageBorder));
        cLog::Info("  -g             show image grid (default: {})", getValue(config.showImageGrid));
        cLog::Info("  -f             start in fullscreen mode (default: {})", getValue(config.fullScreen));
        cLog::Info("  -r             recursive directory scan (default: {})", getValue(config.recursiveScan));
        cLog::Info("  -wz            enable wheel zoom (default: {})", getValue(config.wheelZoom));
        cLog::Info("  -svg SIZE      min SVG rasterization size (default: {} px)", config.minSvgSize);
        cLog::Info("  --             read null-terminated file list from stdin");
        cLog::Info("                 e.g. find /path -name '*.psd' -print0 | sviewgl --");

        cLog::Info("");
        cLog::Info("Key bindings:");
        cLog::Info("  <esc> or <q>       exit");
        cLog::Info("  <space>            next image");
        cLog::Info("  <backspace>        previous image");
        cLog::Info("  <home>             first image");
        cLog::Info("  <end>              last image");
        cLog::Info("  <o>                open file dialog");
        cLog::Info("  <+> / <->          scale image");
        cLog::Info("  <1>...<0>          set scale 100% to 1000%");
        cLog::Info("  <enter>            toggle fullscreen");
        cLog::Info("  <arrows> / <hjkl>  pan image by pixel");
        cLog::Info("  <shift>+<arrows>   pan image by step");
        cLog::Info("  <del>              toggle deletion mark");
        cLog::Info("  <ctrl>+<del>       delete marked images from disk");
        cLog::Info("  <pgup> / <pgdn>    previous / next subimage");
        cLog::Info("  <s>                fit image to window");
        cLog::Info("  <shift>+<s>        toggle keep scale on load");
        cLog::Info("  <r>                rotate clockwise");
        cLog::Info("  <shift>+<r>        rotate counterclockwise");
        cLog::Info("  <f>                flip horizontal");
        cLog::Info("  <shift>+<f>        flip vertical");
        cLog::Info("  <c>                cycle background");
        cLog::Info("  <shift>+<c>        toggle center window mode");
        cLog::Info("  <i>                toggle on-screen info");
        cLog::Info("  <e>                toggle exif popup");
        cLog::Info("  <p>                toggle pixel info");
        cLog::Info("  <b>                toggle image border");
        cLog::Info("  <g>                toggle pixel grid");
        cLog::Info("  <?>                toggle keybindings popup");
    }

} // namespace

extern "C" {
void AddFile(const char* filename)
{
    if (Viewer == nullptr)
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
        Viewer->addPaths(imagesList);
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
        else if (::strncmp(argv[i], "-svg", 4) == 0)
        {
            if (i + 1 < argc)
            {
                config.minSvgSize = static_cast<float>(::atof(argv[++i]));
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
                config.bgColor = { static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), 255 };
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

    cWindow window;
    if (window.init(config) == false)
    {
        return -1;
    }

    cViewer viewer(config, window);
    Viewer = &viewer;
    window.setEventHandler(&viewer.getWindowEvents());

    viewer.addPaths(ImagesList);
    ImagesList.clear();

    // Apply saved window position/size for windowed mode
    if (config.fullScreen == false && config.centerWindow == false)
    {
        window.setSize({ config.windowSize.x, config.windowSize.y });
        window.setPosition({ config.windowPos.x, config.windowPos.y });
    }

    uint32_t frames = 0;
    auto fpsTimer = timing::seconds();

    while (window.shouldClose() == false)
    {
        const auto timeStart = timing::seconds();

        viewer.onRender();
        viewer.onUpdate();

        window.pollEvents();
        viewer.processDeferred();

        frames++;
        const auto now = timing::seconds();
        if (now - fpsTimer >= 1.0)
        {
            viewer.setFps(static_cast<float>(frames / (now - fpsTimer)));
            frames = 0;
            fpsTimer = now;
        }

        constexpr auto desiredFps = 1.0 / 60.0;
        const auto timeRest = desiredFps - (now - timeStart);
        if (timeRest > 0.0)
        {
            usleep(static_cast<useconds_t>(timeRest * 1000000.0));
        }
    }

    Viewer = nullptr;
    fileConfig.write(config);

    return 0;
}

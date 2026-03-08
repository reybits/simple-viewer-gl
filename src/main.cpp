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
#include "common/timing.h"
#include "log/Log.h"
#include "types/types.h"
#include "version.h"
#include "viewer.h"
#include "window.h"

#include <clocale>
#include <cstdio>
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
        printf("%s %d.%d.%d\n\n",
               version::getTitle(),
               version::getMajor(),
               version::getMinor(),
               version::getRelease());
        printf("Copyright © 2008-2025 Andrey A. Ugolnik. All Rights Reserved.\n");
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
        if (helpers::getPlatform() != helpers::Platform::Wayland)
        {
            window.setSize({ config.windowSize.x, config.windowSize.y });
            window.setPosition({ config.windowPos.x, config.windowPos.y });
        }
    }

    while (window.shouldClose() == false)
    {
        const double timeStart = timing::seconds();

        viewer.onRender();
        viewer.onUpdate();

        window.pollEvents();

        if (viewer.isUploading() == false)
        {
            const double frameDuration = timing::seconds() - timeStart;
            constexpr double desiredFps = 1.0 / 60.0;
            const double timeRest = desiredFps - frameDuration;
            if (timeRest > 0.0)
            {
                usleep(static_cast<useconds_t>(timeRest * 1000000));
            }
        }
    }

    Viewer = nullptr;
    fileConfig.write(config);

    return 0;
}

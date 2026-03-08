/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "viewer.h"
#include "checkerboard.h"
#include "common/config.h"
#include "common/helpers.h"
#include "common/timing.h"
#include "deletionmark.h"
#include "fileslist.h"
#include "imageborder.h"
#include "imagegrid.h"
#include "imageloader.h"
#include "log/Log.h"
#include "popups/FileBrowser.h"
#include "popups/exifpopup.h"
#include "popups/helppopup.h"
#include "popups/infobar.h"
#include "popups/pixelpopup.h"
#include "progress.h"
#include "quadimage.h"
#include "selection.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>

namespace
{
    bool AlignScale(int& scale, int step)
    {
        const int oldScale = scale;
        scale /= step;
        scale *= step;
        return oldScale != scale;
    }

} // namespace

cViewer::cViewer(sConfig& config, cWindow& window)
    : m_config(config)
    , m_window(window)
{
    m_windowEvents.onWindowResize = [this](const Vectori& s) { onWindowResize(s); };
    m_windowEvents.onFramebufferResize = [this](const Vectori& s) { onFramebufferResize(s); };
    m_windowEvents.onWindowPosition = [this](const Vectori& p) { onWindowPosition(p); };
    m_windowEvents.onWindowRefresh = [this]() { onWindowRefresh(); };
    m_windowEvents.onKeyEvent = [this](int k, int s, int a, int m) { onKeyEvent(k, s, a, m); };
    m_windowEvents.onCharEvent = [this](uint32_t c) { onCharEvent(c); };
    m_windowEvents.onMouseButton = [this](int b, int a, int m) { onMouseButton(b, a, m); };
    m_windowEvents.onMouseMove = [this](const Vectorf& p) { onMouseMove(p); };
    m_windowEvents.onMouseScroll = [this](const Vectorf& o) { onMouseScroll(o); };
    m_windowEvents.onFileDrop = [this](const StringsList& p) { onFileDrop(p); };

    m_callbacks.startLoading = [this]() { startLoading(); };
    m_callbacks.onBitmapAllocated = [this](const sBitmapDescription& d) { onBitmapAllocated(d); };
    m_callbacks.doProgress = [this](float p) { doProgress(p); };
    m_callbacks.endLoading = [this]() { endLoading(); };

    m_image = std::make_unique<cQuadImage>();
    m_loader = std::make_unique<cImageLoader>(&config, &m_callbacks);
    m_checkerBoard = std::make_unique<cCheckerboard>(config);
    m_deletionMark = std::make_unique<cDeletionMark>();
    m_infoBar = std::make_unique<cInfoBar>(config);
    m_pixelPopup = std::make_unique<cPixelPopup>();
    m_exifPopup = std::make_unique<cExifPopup>();
    m_helpPopup = std::make_unique<cHelpPopup>();
    m_progress = std::make_unique<cProgress>();
    m_border = std::make_unique<cImageBorder>();
    m_grid = std::make_unique<cImageGrid>();
    m_selection = std::make_unique<cSelection>();
    m_filesList = std::make_unique<cFilesList>(config.skipFilter, config.recursiveScan);
    m_fileSelector = std::make_unique<cFileBrowser>();

    onContextRecreated();
}

cViewer::~cViewer()
{
    m_image->clear();

    m_imgui.shutdown();
    render::shutdown();
}

void cViewer::onContextRecreated()
{
    render::init();
    m_imgui.init(m_window);

    m_checkerBoard->init();
    m_deletionMark->init();
    m_pixelPopup->init();
    m_progress->init();
    m_selection->init();

    auto fbSize = m_window.getFramebufferSize();
    auto winSize = m_window.getWindowSize();
    onResize(winSize, fbSize);
}

void cViewer::addPaths(const StringsList& paths)
{
    if (paths.size() != 0)
    {
        for (auto& path : paths)
        {
            m_filesList->addFile(path.c_str());
        }

        m_filesList->sortList();
        m_filesList->locateFile(paths[0].c_str());

        navigateImage(0);
    }
}

void cViewer::onRender()
{
    render::beginFrame();
    m_imgui.beginFrame();

    m_checkerBoard->render();

    const float scale = m_scale.getScale();

    render::setGlobals(m_camera, m_angle, scale);

    m_image->render();

    const auto half_w = static_cast<float>((m_image->getWidth() + 1) >> 1u);
    const auto half_h = static_cast<float>((m_image->getHeight() + 1) >> 1u);

    auto isLoaded = m_loader->isLoaded();
    if (isLoaded)
    {
        if (m_config.showImageBorder)
        {
            m_border->render(-half_w, -half_h, m_image->getWidth(), m_image->getHeight());
        }
        if (m_config.showImageGrid)
        {
            m_grid->render(-half_w, -half_h, m_image->getWidth(), m_image->getHeight());
        }
        if (m_config.showPixelInfo && m_angle == 0)
        {
            m_selection->render({ -half_w, -half_h });
        }
    }
    render::resetGlobals();

    if (isLoaded)
    {
        if (m_config.showExif)
        {
            m_exifPopup->render();
        }

        if (m_filesList->isMarkedForDeletion())
        {
            m_deletionMark->render();
        }

        if (m_config.showPixelInfo && m_cursorInside && m_angle == 0)
        {
            m_pixelPopup->render();
        }
    }

    if (m_config.hideInfobar == false)
    {
        const float progress = m_loadProgress.load(std::memory_order_relaxed);
        if (progress >= 0.0f)
        {
            m_infoBar->setProgressPercent(progress);
        }
        else if (progress > -1.5f)
        {
            // -1.0f means no progress
            m_infoBar->clearProgress();
        }
        else
        {
            // sentinel values for text states
            m_infoBar->setProgressText(progress < -2.5f ? "[decoding...]" : "[loading...]");
        }
        m_infoBar->render();
    }

    m_fileSelector->render();

    m_helpPopup->render();

    m_progress->render();

    m_imgui.endFrame();
    render::endFrame();

    m_window.swapBuffers();
}

void cViewer::onUpdate()
{
    // Progressive upload: start uploading chunks as soon as bitmap is allocated
    if (m_bitmapAllocated.exchange(false))
    {
        const auto& desc = m_loader->getDescription();
        m_image->setBuffer(desc.width, desc.height, desc.pitch, desc.format, desc.bpp, m_loader->getBitmapData());

        if (m_loader->getMode() == cImageLoader::Mode::Image)
        {
            if (m_config.keepScale == false)
            {
                m_scale.setScalePercent(100);
                m_angle = 0;
                m_camera = Vectorf();
            }

            m_selection->setImageDimension(desc.width, desc.height);
            centerWindow();
            enablePixelInfo(m_config.showPixelInfo);
        }

        updateInfobar();
    }

    // Final upload: decode complete (possibly with ICC correction applied)
    if (m_imagePrepared.exchange(false))
    {
        m_uploadFinal = true;

        const auto& desc = m_loader->getDescription();
        // Re-upload with final data (ICC profile may have been applied)
        if (m_image->getWidth() == desc.width
            && m_image->getHeight() == desc.height)
        {
            m_image->refreshData(m_loader->getBitmapData());
        }
        else
        {
            m_image->setBuffer(desc.width, desc.height, desc.pitch, desc.format, desc.bpp, m_loader->getBitmapData());
        }

        if (m_loader->getMode() == cImageLoader::Mode::Image)
        {
            m_exifPopup->setExifList(desc.exifList);
        }

        updateInfobar();
    }

    if (isUploading())
    {
        const uint32_t ready = m_loader->getReadyHeight();
        const bool isDone = m_image->upload(ready);

        // Only show upload progress after at least one chunk has been uploaded,
        // otherwise the decode-phase progress from doProgress() is more accurate.
        const float uploadProgress = m_image->getProgress();
        if (uploadProgress > 0.0f)
        {
            m_loadProgress.store(0.5f + uploadProgress * 0.5f, std::memory_order_relaxed);
        }

        if (isDone)
        {
            const auto& desc = m_loader->getDescription();
            m_progress->hide();
            m_loadProgress.store(-1.0f, std::memory_order_relaxed);
            m_animation = desc.isAnimation;

            // Free bitmap memory — pixel readback now uses GPU textures
            if (m_uploadFinal && !desc.isAnimation && desc.images <= 1)
            {
                m_loader->releaseBitmap();
            }
        }
    }
    else if (m_animation && m_subImageForced == false)
    {
        const auto& desc = m_loader->getDescription();
        if (m_animationTime + desc.delay * 0.001f <= timing::seconds())
        {
            m_animation = false;
            m_animationTime = timing::seconds();
            loadSubImage(1);
        }
    }
}

bool cViewer::isUploading() const
{
    return m_image->isUploading();
}

void cViewer::onResize(const Vectori& winSize, const Vectori& fbSize)
{
    if (winSize.x <= 0 || winSize.y <= 0 || fbSize.x <= 0 || fbSize.y <= 0)
    {
        return;
    }

    m_ratio = { static_cast<float>(fbSize.x) / winSize.x, static_cast<float>(fbSize.y) / winSize.y };

    auto width = std::max(DefaultWindowSize.x, winSize.x);
    auto height = std::max(DefaultWindowSize.y, winSize.y);

    if (m_window.isWindowed())
    {
        m_config.windowSize = { width, height };
    }

    render::setViewportSize(fbSize);

    m_fileSelector->setWindowSize(width, height);

    updatePixelInfo(m_lastMouse);
    updateInfobar();
}

void cViewer::onWindowResize(const Vectori& winSize)
{
    auto fbSize = m_window.getFramebufferSize();
    onResize(winSize, fbSize);
}

void cViewer::onFramebufferResize(const Vectori& fbSize)
{
    auto winSize = m_window.getWindowSize();
    onResize(winSize, fbSize);
}

void cViewer::onWindowPosition(const Vectori& pos)
{
    m_config.windowPos = pos;
}

void cViewer::onWindowRefresh()
{
    onRender();
}

Vectorf cViewer::calculateMousePosition(const Vectorf& pos) const
{
    return pos * m_ratio / m_scale.getScale();
}

void cViewer::onMouseMove(const Vectorf& pos)
{
    m_imgui.onMousePosition(pos);

    if (m_fileSelector->isVisible())
    {
        return;
    }

    const auto posFixed = calculateMousePosition(pos);

    if (m_mouseMB || m_mouseRB)
    {
        const Vectorf diff(m_lastMouse - posFixed);
        m_lastMouse = posFixed;

        if (diff != Vectorf())
        {
            shiftCamera(diff);
        }
    }

    auto isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
    updateCursorState(isHovered || m_config.showPixelInfo == false);
    if (m_config.showPixelInfo)
    {
        const int cursor = m_selection->getCursor();
        m_pixelPopup->setCursor(cursor);

        const Vectorf point = screenToImage(posFixed);
        m_selection->mouseMove(point, m_scale.getScale());

        updatePixelInfo(posFixed);
    }
}

void cViewer::onMouseScroll(const Vectorf& offset)
{
    m_imgui.onScroll(offset);

    if (m_fileSelector->isVisible())
    {
        return;
    }

    if (m_config.wheelZoom)
    {
        updateScale(offset.y > 0.0f
                        ? ScaleDirection::Up
                        : ScaleDirection::Down);
    }
    else
    {
        auto panRatio = m_ratio * m_config.panRatio;
        shiftCamera({ -offset.x * panRatio.x, -offset.y * panRatio.y });
    }
}

void cViewer::onMouseButton(int button, int action, int /*mods*/)
{
    m_imgui.onMouseButton(button, action);

    if (m_fileSelector->isVisible())
    {
        return;
    }

    updateMousePosition();

    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
        {
            auto pressed = (action == GLFW_PRESS);
            auto isHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow);
            const Vectorf point = screenToImage(m_lastMouse);
            m_selection->mouseButton(point, m_scale.getScale(), pressed && isHovered == false);

            auto& rect = m_selection->getRect();
            if (rect.isSet() == false)
            {
                updatePixelInfo(m_lastMouse);
            }
        }
        break;

    case GLFW_MOUSE_BUTTON_MIDDLE:
        m_mouseMB = (action == GLFW_PRESS);
        break;

    case GLFW_MOUSE_BUTTON_RIGHT:
        m_mouseRB = (action == GLFW_PRESS);
        break;
    }
}

void cViewer::onKeyEvent(int key, int scancode, int action, int mods)
{
    m_imgui.onKey(key, scancode, action);

    if (m_fileSelector->isVisible())
    {
        return;
    }

    if (action != GLFW_PRESS && action != GLFW_REPEAT)
    {
        return;
    }

    switch (key)
    {
    case GLFW_KEY_SLASH:
        if (mods & GLFW_MOD_SHIFT)
        {
            m_helpPopup->show(!m_helpPopup->isVisible());
        }
        break;

    case GLFW_KEY_ESCAPE:
    case GLFW_KEY_Q:
        if (m_fileSelector->isVisible() == false)
        {
            m_window.requestClose();
        }
        else
        {
            m_fileSelector->close();
        }
        break;

    case GLFW_KEY_O:
        if (m_fileSelector->isVisible() == false)
        {
            auto currentFile = m_filesList->getName();
            auto dir = helpers::getDirectoryFromPath(currentFile);
            m_fileSelector->setWindowSize(m_config.windowSize.x, m_config.windowSize.y);
            m_fileSelector->open(
                [this](cFileBrowser::Result result) {
                    auto path = result.directory + "/" + result.fileName;

                    m_filesList->addFile(path.c_str());
                    loadImage(path.c_str());
                },
                cFileBrowser::Type::Open,
                "Open Image",
                dir);
        }
        break;

    case GLFW_KEY_I:
        m_config.hideInfobar = !m_config.hideInfobar;
        centerWindow();
        break;

    case GLFW_KEY_E:
        m_config.showExif = !m_config.showExif;
        break;

    case GLFW_KEY_P:
        enablePixelInfo(!m_config.showPixelInfo);
        break;

    case GLFW_KEY_S:
        if (mods & GLFW_MOD_SHIFT)
        {
            m_config.keepScale = !m_config.keepScale;
        }
        else
        {
            m_config.fitImage = !m_config.fitImage;
            if (m_config.fitImage == false)
            {
                resetViewAndUpdate(100);
            }
            else
            {
                m_camera = Vectorf();
                centerWindow();
                updateInfobar();
            }
        }
        break;

    case GLFW_KEY_SPACE:
        navigateImage(1);
        break;

    case GLFW_KEY_BACKSPACE:
        navigateImage(-1);
        break;

    case GLFW_KEY_HOME:
        loadFirstImage();
        break;

    case GLFW_KEY_END:
        loadLastImage();
        break;

    case GLFW_KEY_DELETE:
        if (mods & GLFW_MOD_CONTROL)
        {
            const bool marked = m_filesList->isMarkedForDeletion();
            m_filesList->removeMarkedFromDisk();

            if (marked)
            {
                navigateImage(0);
            }
        }
        else
        {
            m_filesList->toggleDeletionMark();
        }
        break;

    case GLFW_KEY_B:
        m_config.showImageBorder = !m_config.showImageBorder;
        break;

    case GLFW_KEY_G:
        m_config.showImageGrid = !m_config.showImageGrid;
        break;

    case GLFW_KEY_EQUAL:
    case GLFW_KEY_KP_ADD:
        updateScale(ScaleDirection::Up);
        break;

    case GLFW_KEY_MINUS:
    case GLFW_KEY_KP_SUBTRACT:
        updateScale(ScaleDirection::Down);
        break;

    case GLFW_KEY_C:
        if (mods & GLFW_MOD_SHIFT)
        {
            m_config.centerWindow = !m_config.centerWindow;
            if (m_config.centerWindow)
            {
                centerWindow();
            }
        }
        else
        {
            m_config.backgroundIndex = (m_config.backgroundIndex + 1) % 5;
        }
        break;

    case GLFW_KEY_ENTER:
    case GLFW_KEY_KP_ENTER:
    {
        m_window.toggleFullscreen(m_config);
        onContextRecreated();
        break;
    }

    case GLFW_KEY_H:
    case GLFW_KEY_LEFT:
        keyLeft((mods & GLFW_MOD_SHIFT) == 0);
        break;

    case GLFW_KEY_L:
    case GLFW_KEY_RIGHT:
        keyRight((mods & GLFW_MOD_SHIFT) == 0);
        break;

    case GLFW_KEY_K:
    case GLFW_KEY_UP:
        keyUp((mods & GLFW_MOD_SHIFT) == 0);
        break;

    case GLFW_KEY_J:
    case GLFW_KEY_DOWN:
        keyDown((mods & GLFW_MOD_SHIFT) == 0);
        break;

    case GLFW_KEY_R:
        if (mods & GLFW_MOD_SHIFT)
        {
            m_angle = (m_angle + 90) % 360;
        }
        else
        {
            m_angle = (m_angle + 360 - 90) % 360;
        }
        updateCursorState(m_angle == 0
                              ? m_config.showPixelInfo == false
                              : true);
        calculateScale();
        break;

    case GLFW_KEY_PAGE_UP:
        m_subImageForced = true;
        loadSubImage(-1);
        break;

    case GLFW_KEY_PAGE_DOWN:
        m_subImageForced = true;
        loadSubImage(1);
        break;

    default:
        if (key == GLFW_KEY_0)
        {
            resetViewAndUpdate(1000);
        }
        else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9)
        {
            resetViewAndUpdate((key - GLFW_KEY_0) * 100);
        }
        break;
    }
}

void cViewer::onCharEvent(uint32_t c)
{
    m_imgui.onChar(c);
}

void cViewer::onFileDrop(const StringsList& paths)
{
    addPaths(paths);
}

float cViewer::getStepVert(bool byPixel) const
{
    if (byPixel)
    {
        return m_config.shiftInPixels / m_scale.getScale();
    }

    auto percent = m_config.shiftInPercent;
    return percent * m_image->getHeight();
}

float cViewer::getStepHori(bool byPixel) const
{
    if (byPixel)
    {
        return m_config.shiftInPixels / m_scale.getScale();
    }

    auto percent = m_config.shiftInPercent;
    return percent * m_image->getWidth();
}

void cViewer::keyUp(bool byPixel)
{
    auto step = getStepVert(byPixel);
    shiftCamera({ 0.0f, -step });
}

void cViewer::keyDown(bool byPixel)
{
    auto step = getStepVert(byPixel);
    shiftCamera({ 0.0f, step });
}

void cViewer::keyLeft(bool byPixel)
{
    auto step = getStepHori(byPixel);
    shiftCamera({ -step, 0.0f });
}

void cViewer::keyRight(bool byPixel)
{
    auto step = getStepHori(byPixel);
    shiftCamera({ step, 0.0f });
}

void cViewer::shiftCamera(const Vectorf& delta)
{
    m_camera += delta;

    const float inv = 1.0f / m_scale.getScale();
    const auto& viewport = render::getViewportSize();
    const auto half = Vectorf(viewport.x * inv + m_image->getWidth(), viewport.y * inv + m_image->getHeight()) * 0.5f;
    m_camera.x = std::clamp(m_camera.x, -half.x, half.x);
    m_camera.y = std::clamp(m_camera.y, -half.y, half.y);
}

void cViewer::calculateScale()
{
    if (m_config.fitImage && m_loader->isLoaded())
    {
        auto w = static_cast<float>(m_image->getWidth());
        auto h = static_cast<float>(m_image->getHeight());
        if (m_angle == 90 || m_angle == 270)
        {
            std::swap(w, h);
        }

        const auto& viewport = render::getViewportSize();
        if (w >= viewport.x || h >= viewport.y)
        {
            auto aspect = w / h;
            auto dx = w / viewport.x;
            auto dy = h / viewport.y;
            auto new_w = 0.0f;
            auto new_h = 0.0f;
            if (dx > dy)
            {
                if (w > viewport.x)
                {
                    new_w = viewport.x;
                    new_h = new_w / aspect;
                }
            }
            else
            {
                if (h > viewport.y)
                {
                    new_h = viewport.y;
                    new_w = new_h * aspect;
                }
            }
            if (new_w != 0.0f && new_h != 0.0f)
            {
                m_scale.setScale(new_w / w);
            }
        }
        else
        {
            m_scale.setScalePercent(100);
        }
    }

    updateFiltering();
}

// TODO: update m_camera according to current mouse position (zoom to cursor)
void cViewer::updateScale(ScaleDirection direction)
{
    m_config.fitImage = false;

    int scale = m_scale.getScalePercent();

    if (direction == ScaleDirection::Up)
    {
        const int step = scale >= 100 ? 25 : (scale >= 50 ? 10 : (scale >= 30 ? 5 : 1));
        AlignScale(scale, step);
        scale += step;
    }
    else if (direction == ScaleDirection::Down)
    {
        const int step = scale > 100 ? 25 : (scale > 50 ? 10 : (scale > 30 ? 5 : 1));
        if (AlignScale(scale, step) == false && scale > step)
        {
            scale -= step;
        }
    }

    m_scale.setScalePercent(scale);

    updateFiltering();
    updateInfobar();
}

void cViewer::updateFiltering()
{
    const int scale = m_scale.getScalePercent();
    if (scale >= 100 && scale % 100 == 0)
    {
        m_image->useFilter(false);
    }
    else
    {
        m_image->useFilter(true);
    }
}

void cViewer::centerWindow()
{
    if (m_window.isWindowed())
    {
        if (helpers::getPlatform() != helpers::Platform::Wayland)
        {
            auto width = m_config.windowSize.w;
            auto height = m_config.windowSize.h;

            if (m_config.centerWindow)
            {
                auto screen = m_window.getScreenSize();

                // calculate image size with border
                auto tickness = m_config.showImageBorder
                    ? m_border->getThickness() * 2
                    : 0.0f;
                auto scale = m_scale.getScale();

                // When fit-to-window is enabled for large images,
                // pre-calculate the fit scale so the window matches the fitted size
                if (m_config.fitImage && m_loader->isLoaded())
                {
                    auto w = static_cast<float>(m_image->getWidth());
                    auto h = static_cast<float>(m_image->getHeight());
                    auto maxW = screen.x * m_ratio.x;
                    auto maxH = screen.y * m_ratio.y;
                    if (w > maxW || h > maxH)
                    {
                        scale = std::min(maxW / w, maxH / h);
                    }
                }

                auto imgw = m_image->getWidth() * scale + tickness;
                auto imgh = m_image->getHeight() * scale + tickness;

                width = std::max<int>(imgw / m_ratio.x, DefaultWindowSize.w);
                height = std::max<int>(imgh / m_ratio.y, DefaultWindowSize.h);

                // clamp to screen size
                width = std::min<int>(width, screen.x);
                height = std::min<int>(height, screen.y);

                // Apply initial size to get the actual viewport
                // (macOS may constrain due to menu bar, title bar, dock)
                m_config.windowSize = { width, height };
                m_window.setSize({ width, height });
                calculateScale();

                // Recompute window size using the actual fit scale
                // (viewport may be smaller than pre-calculated estimate)
                if (m_config.fitImage && m_loader->isLoaded())
                {
                    scale = m_scale.getScale();
                    imgw = m_image->getWidth() * scale + tickness;
                    imgh = m_image->getHeight() * scale + tickness;

                    width = std::max<int>(imgw / m_ratio.x, DefaultWindowSize.w);
                    height = std::max<int>(imgh / m_ratio.y, DefaultWindowSize.h);

                    width = std::min<int>(width, screen.x);
                    height = std::min<int>(height, screen.y);

                    m_config.windowSize = { width, height };
                }

                // center and apply final size
                auto x = (screen.x - width) / 2;
                auto y = (screen.y - height) / 2;
                m_config.windowPos = { x, y };
                m_window.setPosition({ x, y });
            }

            m_window.setSize({ width, height });
        }

        calculateScale();
    }
}

void cViewer::resetViewAndUpdate(int scalePercent)
{
    m_scale.setScalePercent(scalePercent);
    m_camera = Vectorf();
    m_config.fitImage = false;
    centerWindow();
    updateInfobar();
}

void cViewer::loadFirstImage()
{
    auto path = m_filesList->getFirstName();
    loadImage(path);
}

void cViewer::loadLastImage()
{
    auto path = m_filesList->getLastName();
    loadImage(path);
}

void cViewer::navigateImage(int step)
{
    auto path = m_filesList->getName(step);
    loadImage(path);
}

void cViewer::loadImage(const char* path)
{
    m_config.fitImage = m_config.keepScale == false && m_config.fitImage;

    m_subImageForced = false;
    m_animation = false;
    m_image->stop();

    m_loader->loadImage(path);
    updateInfobar();
}

void cViewer::loadSubImage(int subStep)
{
    assert(subStep == -1 || subStep == 1);

    m_animation = false;
    m_image->stop();

    const auto& desc = m_loader->getDescription();
    const unsigned next = (desc.current + desc.images + subStep) % desc.images;
    if (desc.current != next)
    {
        m_loader->loadSubImage(next);
    }
}

void cViewer::updateInfobar()
{
    calculateScale();

    cInfoBar::sInfo s;
    s.path = m_filesList->getName();
    s.scale = m_scale.getScale();
    s.index = m_filesList->getIndex();
    s.files_count = m_filesList->getCount();

    if (m_loader->isLoaded())
    {
        const auto& desc = m_loader->getDescription();
        s.width = desc.width;
        s.height = desc.height;
        s.bpp = desc.bppImage;
        s.images = desc.images;
        s.current = desc.current;
        s.file_size = desc.size;
        s.mem_size = desc.bitmap.empty() ? desc.bitmapSize : desc.bitmap.size();
        s.type = m_loader->getImageType();
    }
    else
    {
        s.type = "unknown";
    }
    m_infoBar->setInfo(s);

    // Set window title to current filename
    if (s.path != nullptr)
    {
        const char* name = s.path;
        const char* p = std::strrchr(s.path, '/');
        if (p != nullptr)
        {
            name = p + 1;
        }
        m_window.setTitle(name);
    }
}

Vectorf cViewer::screenToImage(const Vectorf& pos) const
{
    const auto& viewport = render::getViewportSize();
    auto scale = m_scale.getScale();
    auto size = Vectorf{ viewport.x / scale - m_image->getWidth(),
                         viewport.y / scale - m_image->getHeight() };
    return pos + m_camera - size * 0.5f;
}

void cViewer::updatePixelInfo(const Vectorf& pos)
{
    sPixelInfo pixelInfo;

    const Vectorf point = screenToImage(pos);

    pixelInfo.mouse = pos * m_scale.getScale();
    pixelInfo.point = point;

    if (m_loader->isLoaded())
    {
        const auto& desc = m_loader->getDescription();
        pixelInfo.bpp = desc.bpp;

        const int x = static_cast<int>(point.x);
        const int y = static_cast<int>(point.y);

        if (x >= 0 && y >= 0
            && static_cast<uint32_t>(x) < m_image->getWidth()
            && static_cast<uint32_t>(y) < m_image->getHeight())
        {
            m_image->getPixel(static_cast<uint32_t>(x), static_cast<uint32_t>(y), pixelInfo.color);
        }

        pixelInfo.imgWidth = m_image->getWidth();
        pixelInfo.imgHeight = m_image->getHeight();
        pixelInfo.rc = m_selection->getRect();
    }

    m_pixelPopup->setPixelInfo(pixelInfo);
}

void cViewer::updateCursorState(bool show)
{
    auto cursorPos = m_window.getCursorPos();

    auto& size = m_config.windowSize;
    m_cursorInside = !(cursorPos.x < 0.0f || cursorPos.x >= size.w || cursorPos.y < 0.0f || cursorPos.y >= size.h);

    m_window.setCursorVisible(show);
}

void cViewer::startLoading()
{
    const auto& desc = m_loader->getDescription();
    if (desc.isAnimation == false)
    {
        m_progress->show();
    }
    m_loadProgress.store(-2.0f, std::memory_order_relaxed); // "loading..."
    m_bitmapAllocated.store(false, std::memory_order_relaxed);
    m_imagePrepared.store(false, std::memory_order_relaxed);
    m_uploadFinal = false;
}

void cViewer::onBitmapAllocated(const sBitmapDescription& /*desc*/)
{
    m_loadProgress.store(-3.0f, std::memory_order_relaxed); // "decoding..."
    m_bitmapAllocated.store(true, std::memory_order_release);
}

void cViewer::doProgress(float progress)
{
    m_loadProgress.store(progress * 0.5f, std::memory_order_relaxed);
}

void cViewer::endLoading()
{
    m_imagePrepared.store(true, std::memory_order_release);
}

void cViewer::updateMousePosition()
{
    auto cursorPos = m_window.getCursorPos();
    m_lastMouse = calculateMousePosition(cursorPos);
}

void cViewer::enablePixelInfo(bool show)
{
    if (show)
    {
        updateMousePosition();
        updatePixelInfo(m_lastMouse);
    }
    m_config.showPixelInfo = show;
    updateCursorState(show == false);
}

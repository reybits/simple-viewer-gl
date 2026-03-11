/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "Viewer.h"
#include "Checkerboard.h"
#include "Common/Config.h"
#include "Common/Helpers.h"
#include "Common/Timing.h"
#include "DeletionMark.h"
#include "FilesList.h"
#include "Gui.h"
#include "ImageBorder.h"
#include "ImageGrid.h"
#include "ImageLoader.h"
#include "Log/Log.h"
#include "Popups/ExifPopup.h"
#include "Popups/FileBrowser.h"
#include "Popups/HelpPopup.h"
#include "Popups/InfoBar.h"
#include "Popups/PixelPopup.h"
#include "Progress.h"
#include "QuadImage.h"
#include "Selection.h"

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
    m_callbacks.onImageInfo = [this](const sChunkData& c, const sImageInfo& i) { onImageInfo(c, i); };
    m_callbacks.onPreviewReady = [this](sPreviewData&& p) { onPreviewReady(std::move(p)); };
    m_callbacks.onBitmapAllocated = [this](const sChunkData& c) { onBitmapAllocated(c); };
    m_callbacks.doProgress = [this](float p) { doProgress(p); };
    m_callbacks.endLoading = [this]() { endLoading(); };

    m_image = std::make_unique<cQuadImage>();
    m_loader = std::make_unique<cImageLoader>(&config, &m_callbacks);
    m_checkerBoard = std::make_unique<cCheckerboard>(config);
    m_deletionMark = std::make_unique<cDeletionMark>();
    m_infoBar = std::make_unique<cInfoBar>(config);
    m_imgui = std::make_unique<cGui>(window, config);
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

    m_imgui.reset();
    render::shutdown();
}

void cViewer::onContextRecreated()
{
    render::init();

    m_checkerBoard->init();
    m_pixelPopup->init();
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
    // Sync viewport with current framebuffer size to avoid stale projection.
    render::setViewportSize(m_window.getFramebufferSize());

    render::beginFrame();
    m_imgui->setInfoBarVisible(m_config.hideInfobar == false);
    m_imgui->beginFrame();

    // Recalculate scale after dock layout is up-to-date.
    calculateScale();

    m_checkerBoard->render();

    const float scale = m_scale.getScale();

    render::setGlobals(getAdjustedCamera(), m_angle, scale, m_flipH, m_flipV);

    if (m_preview != nullptr)
    {
        auto fw = static_cast<float>(m_previewData.fullImageWidth);
        auto fh = static_cast<float>(m_previewData.fullImageHeight);

        // When fitImage is active, compute the fit scale from the full-res
        // dimensions directly — m_image may still have old dimensions before
        // handleBitmapAllocated() runs.  When fitImage is off, scale is a
        // user-set percentage (image-size-independent), so use it as-is.
        auto displayScale = scale;
        if (m_config.fitImage)
        {
            const auto centralFb = getCentralAreaFbSize();
            displayScale = (fw >= centralFb.x || fh >= centralFb.y)
                ? std::min(centralFb.x / fw, centralFb.y / fh)
                : 1.0f;
        }

        const auto previewScale = displayScale * fw / static_cast<float>(m_preview->getWidth());
        render::setGlobals(getAdjustedCamera(previewScale), m_angle, previewScale, m_flipH, m_flipV);
        m_preview->render();
        render::setGlobals(getAdjustedCamera(), m_angle, scale, m_flipH, m_flipV);
    }

    m_image->render();

    auto isLoaded = m_loader->isLoaded();
    if (isLoaded)
    {
        const auto half_w = static_cast<float>((m_image->getWidth() + 1) >> 1u);
        const auto half_h = static_cast<float>((m_image->getHeight() + 1) >> 1u);

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
        m_infoBar->render();
    }

    m_fileSelector->render();

    m_helpPopup->render();

    {
        const float progress = m_loadProgress.load(std::memory_order_relaxed);
        if (progress >= 0.0f)
        {
            m_progress->setPercent(progress);
        }
        else if (progress > -1.5f)
        {
            m_progress->clearStatus();
        }
        else if (progress < -3.5f)
        {
            m_progress->setStatus("post-processing...");
        }
        else
        {
            m_progress->setStatus(progress < -2.5f ? "decoding..." : "loading...");
        }

        const auto& centralPos = m_imgui->getCentralPos();
        const auto& centralSize = m_imgui->getCentralSize();
        m_progress->render(centralPos.x + centralSize.x, centralPos.y + centralSize.y);
    }

    m_imgui->endFrame();
    render::endFrame();

    m_window.swapBuffers();
}

void cViewer::onUpdate()
{
    if (m_imageInfoReady.exchange(false))
    {
        updateInfobar();
    }

    bool previewJustShown = false;
    if (m_previewReady.exchange(false))
    {
        handlePreviewReady();
        previewJustShown = true;
    }

    if (m_bitmapAllocated.exchange(false))
    {
        if (previewJustShown)
        {
            // Defer by one frame so the preview texture has time to upload.
            m_bitmapAllocated.store(true, std::memory_order_relaxed);
        }
        else
        {
            handleBitmapAllocated();
        }
    }

    // Only handle imagePrepared after bitmapAllocated has been processed,
    // otherwise handleImageReady() runs before the image buffer is set up.
    if (m_bitmapAllocated.load(std::memory_order_relaxed) == false
        && m_imagePrepared.exchange(false))
    {
        handleImageReady();
    }

    if (isUploading())
    {
        const uint32_t ready = m_loader->getReadyHeight();
        const bool isDone = m_image->upload(ready);
        m_loader->setConsumedHeight(ready);

        const float uploadProgress = m_image->getProgress();
        if (uploadProgress > 0.0f)
        {
            m_loadProgress.store(uploadProgress, std::memory_order_relaxed);
        }

        if (isDone)
        {
            if (m_config.debug)
            {
                const double uploadMs = (timing::seconds() - m_uploadStartTime) * 1000.0;
                cLog::Debug("  upload {}x{}: {:.1f} ms  (chunk {}x{} grid {}x{}) [{}]",
                            m_image->getWidth(), m_image->getHeight(),
                            uploadMs,
                            m_image->getTexWidth(), m_image->getTexHeight(),
                            m_image->getCols(), m_image->getRows(),
                            m_uploadFinal ? "final" : "progressive");
            }

            if (m_uploadFinal)
            {
                m_progress->hide();
                m_loadProgress.store(-1.0f, std::memory_order_relaxed);
            }
            else
            {
                m_loadProgress.store(-4.0f, std::memory_order_relaxed); // "[post-processing...]"
            }

            const auto& uploadInfo = m_loader->getImageInfo();
            m_animation = uploadInfo.isAnimation;

            // Full-res upload complete — discard preview.
            if (m_preview != nullptr)
            {
                m_preview.reset();
                m_previewData = {};
            }

            // Free bitmap memory — pixel readback now uses GPU textures.
            if (m_uploadFinal && !uploadInfo.isAnimation && uploadInfo.images <= 1)
            {
                m_loader->releaseBitmap();
                updateInfobar();
            }
        }
    }
    else if (m_animation && m_subImageForced == false)
    {
        const auto& animInfo = m_loader->getImageInfo();
        if (m_animationTime + animInfo.delay * 0.001f <= timing::seconds())
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

void cViewer::setFps(float fps)
{
    m_imgui->setFps(fps);
}

void cViewer::handlePreviewReady()
{
    auto& p = m_previewData;
    m_preview = std::make_unique<cQuadImage>();
    m_preview->setBuffer(p.width, p.height, p.pitch, p.format, p.bpp, p.bitmap.data());
    m_preview->upload(p.height);
}

void cViewer::handleBitmapAllocated()
{
    m_uploadActive.store(true, std::memory_order_relaxed);
    m_uploadStartTime = timing::seconds();

    const auto& chunk = m_loader->getChunkData();
    m_image->setBuffer(chunk.width, chunk.height, chunk.pitch, chunk.format, chunk.bpp, m_loader->getBitmapData(), chunk.bandHeight);

    if (chunk.lutData.empty() == false)
    {
        m_image->setLutData(chunk.lutData, chunk.lutSize);
    }

    if (m_loader->getMode() == cImageLoader::Mode::Image)
    {
        if (m_config.keepScale == false)
        {
            m_scale.setScalePercent(100);
            resetOrientation();
            m_camera = Vectorf();
        }

        m_selection->setImageDimension(chunk.width, chunk.height);
        centerWindow();
        enablePixelInfo(m_config.showPixelInfo);
    }

    updateInfobar();
}

void cViewer::handleImageReady()
{
    const auto& chunk = m_loader->getChunkData();
    const auto& info = m_loader->getImageInfo();

    if (m_config.debug)
    {
        const auto& met = m_loader->getMetrics();
        cLog::Debug("--- {} {}x{} {}-bit ---",
                    info.formatName ? info.formatName : "?",
                    chunk.width, chunk.height, info.bppImage);
        cLog::Debug("  file read:  {:.1f} ms", met.fileReadMs);
        cLog::Debug("  decode:     {:.1f} ms", met.decodeMs);
        if (met.iccMs > 0.0)
        {
            cLog::Debug("  icc:        {:.1f} ms", met.iccMs);
        }
        cLog::Debug("  total load: {:.1f} ms", met.totalMs);
        cLog::Debug("  bitmap:     {:.1f} MB", met.bitmapBytes / (1024.0 * 1024.0));
    }

    // Formats that don't call signalBitmapAllocated() (e.g., AGE) never trigger
    // handleBitmapAllocated(), so the GPU buffer was never set up. Do it now.
    if (m_image->getWidth() == 0 && chunk.width > 0)
    {
        m_uploadActive.store(true, std::memory_order_relaxed);
        m_uploadStartTime = timing::seconds();
        m_image->setBuffer(chunk.width, chunk.height, chunk.pitch, chunk.format, chunk.bpp, m_loader->getBitmapData());

        if (m_loader->getMode() == cImageLoader::Mode::Image)
        {
            if (m_config.keepScale == false)
            {
                m_scale.setScalePercent(100);
                resetOrientation();
                m_camera = Vectorf();
            }

            m_selection->setImageDimension(chunk.width, chunk.height);
            centerWindow();
            enablePixelInfo(m_config.showPixelInfo);
        }
    }
    else if (isUploading() == false)
    {
        // Progressive upload already completed and no re-upload needed.
        m_progress->hide();
        m_loadProgress.store(-1.0f, std::memory_order_relaxed);
    }

    // Wire LUT for batch ICC formats (generated after decode, not at allocation time)
    if (chunk.lutData.empty() == false && m_image->hasLut() == false)
    {
        m_image->setLutData(chunk.lutData, chunk.lutSize);
    }

    if (m_loader->getMode() == cImageLoader::Mode::Image)
    {
        m_exifPopup->setExifList(info.exifList);

        // Reset orientation before applying EXIF — orientation is intrinsic
        // to the image, not a user preference that should persist across images.
        resetOrientation();
        applyExifOrientation(info.exifOrientation);
    }

    updateInfobar();
}

void cViewer::resetOrientation()
{
    m_angle = 0;
    m_flipH = false;
    m_flipV = false;
}

void cViewer::applyExifOrientation(uint16_t orientation)
{
    // EXIF orientation values:
    // 1=normal, 2=flipH, 3=rotate180, 4=flipV,
    // 5=rotate90CW+flipH, 6=rotate90CW, 7=rotate90CCW+flipH, 8=rotate90CCW
    switch (orientation)
    {
    case 2:
        m_flipH = true;
        break;
    case 3:
        m_angle = 180;
        break;
    case 4:
        m_flipV = true;
        break;
    case 5:
        m_angle = 270;
        m_flipH = true;
        break;
    case 6:
        m_angle = 270;
        break;
    case 7:
        m_angle = 90;
        m_flipH = true;
        break;
    case 8:
        m_angle = 90;
        break;
    }
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
    m_imgui->onMousePosition(pos);

    if (m_fileSelector->isVisible() || m_imgui->wantCaptureMouse())
    {
        updateCursorState(true);
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
    m_imgui->onScroll(offset);

    if (m_fileSelector->isVisible() || m_imgui->wantCaptureMouse())
    {
        return;
    }

    if (m_config.wheelZoom)
    {
        auto cursorPos = m_window.getCursorPos();
        Vectorf cursorFb = cursorPos * m_ratio;
        updateScale(offset.y > 0.0f
                        ? ScaleDirection::Up
                        : ScaleDirection::Down,
                    &cursorFb);
    }
    else
    {
        auto panRatio = m_ratio * m_config.panRatio / m_scale.getScale();
        shiftCamera({ -offset.x * panRatio.x, -offset.y * panRatio.y });
    }
}

void cViewer::onMouseButton(int button, int action, int /*mods*/)
{
    m_imgui->onMouseButton(button, action);

    if (m_fileSelector->isVisible() || m_imgui->wantCaptureMouse())
    {
        return;
    }

    updateMousePosition();

    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT: {
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
    m_imgui->onKey(key, scancode, action);

    if (m_fileSelector->isVisible() || m_imgui->wantCaptureKeyboard())
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
    case GLFW_KEY_KP_ENTER: {
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
        break;

    case GLFW_KEY_F:
        if (mods & GLFW_MOD_SHIFT)
        {
            m_flipV = !m_flipV;
        }
        else
        {
            m_flipH = !m_flipH;
        }
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
    m_imgui->onChar(c);

    // Char events are only relevant for ImGui text input — no viewer handling needed.
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
    clampCamera();
}

void cViewer::clampCamera()
{
    const float scale = m_scale.getScale();
    const auto centralFb = getCentralAreaFbSize();
    const float halfVpW = centralFb.x * 0.5f / scale;
    const float halfVpH = centralFb.y * 0.5f / scale;
    const float halfImgW = m_image->getWidth() * 0.5f;
    const float halfImgH = m_image->getHeight() * 0.5f;

    if (m_config.fitImage)
    {
        m_camera = Vectorf();
        return;
    }

    // Image larger than viewport: pan until edge reaches ~25% from viewport center
    // Image smaller than viewport: allow nudging off-center by ~25% of viewport
    const float limitX = std::max(halfImgW - halfVpW, 0.0f) + halfVpW * 0.25f;
    const float limitY = std::max(halfImgH - halfVpH, 0.0f) + halfVpH * 0.25f;
    m_camera.x = std::clamp(m_camera.x, -limitX, limitX);
    m_camera.y = std::clamp(m_camera.y, -limitY, limitY);
}

void cViewer::calculateScale()
{
    if (m_config.fitImage && m_image->getWidth() > 0 && m_image->getHeight() > 0)
    {
        auto w = static_cast<float>(m_image->getWidth());
        auto h = static_cast<float>(m_image->getHeight());
        if (m_angle == 90 || m_angle == 270)
        {
            std::swap(w, h);
        }

        const auto centralFb = getCentralAreaFbSize();
        const float vpW = centralFb.x;
        const float vpH = centralFb.y;
        if (w >= vpW || h >= vpH)
        {
            auto aspect = w / h;
            auto dx = w / vpW;
            auto dy = h / vpH;
            auto new_w = 0.0f;
            auto new_h = 0.0f;
            if (dx > dy)
            {
                if (w > vpW)
                {
                    new_w = vpW;
                    new_h = new_w / aspect;
                }
            }
            else
            {
                if (h > vpH)
                {
                    new_h = vpH;
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

Vectorf cViewer::getCentralAreaFbSize() const
{
    return m_imgui->getCentralSize() * m_ratio;
}

Vectorf cViewer::getCentralAreaFbCenter() const
{
    auto pos = m_imgui->getCentralPos() * m_ratio;
    auto size = getCentralAreaFbSize();
    return { pos.x + size.x * 0.5f, pos.y + size.y * 0.5f };
}

Vectorf cViewer::getAdjustedCamera(float scaleOverride) const
{
    const auto& viewport = render::getViewportSize();
    const auto centralCenter = getCentralAreaFbCenter();
    const float s = (scaleOverride > 0.0f)
        ? scaleOverride
        : m_scale.getScale();
    return { m_camera.x - (centralCenter.x - viewport.x * 0.5f) / s,
             m_camera.y - (centralCenter.y - viewport.y * 0.5f) / s };
}

void cViewer::updateScale(ScaleDirection direction, const Vectorf* cursorFb)
{
    m_config.fitImage = false;

    const float oldScale = m_scale.getScale();

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

    // Zoom to cursor: keep the point under the cursor stationary
    if (cursorFb)
    {
        const float newScale = m_scale.getScale();
        const float invDelta = 1.0f / oldScale - 1.0f / newScale;
        const Vectorf diff = *cursorFb - getCentralAreaFbCenter();
        m_camera -= diff * invDelta;
        clampCamera();
    }

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
    if (m_window.isWindowed() == false
        || helpers::getPlatform() == helpers::Platform::Wayland)
    {
        return;
    }

    // When both centerWindow and fitImage are off, the user has full manual
    // control over window size and zoom — nothing to do.
    if (m_config.centerWindow == false && m_config.fitImage == false)
    {
        return;
    }

    auto screen = m_window.getScreenSize();

    // Compute overhead from docked windows + infobar.
    // The central area (from the last frame) already excludes these.
    const auto centralSize = m_imgui->getCentralSize();
    float overheadX = 0.0f;
    float overheadY = m_config.hideInfobar
        ? 0.0f
        : m_imgui->getInfoBarHeight();

    if (centralSize.x > 0.0f && centralSize.y > 0.0f)
    {
        const auto winSize = m_window.getWindowSize();
        overheadX = static_cast<float>(winSize.x) - centralSize.x;
        overheadY = static_cast<float>(winSize.y) - centralSize.y;
    }

    auto tickness = m_config.showImageBorder
        ? m_border->getThickness() * 2
        : 0.0f;
    auto scale = m_scale.getScale();

    auto imgWidth = static_cast<float>(m_image->getWidth());
    auto imgHeight = static_cast<float>(m_image->getHeight());
    if (m_angle == 90 || m_angle == 270)
    {
        std::swap(imgWidth, imgHeight);
    }

    // For fit-to-window: compute scale from the maximum available area
    // (screen minus overhead from infobar and docked windows).
    if (m_config.fitImage && m_image->getWidth() > 0 && m_image->getHeight() > 0)
    {
        auto maxW = (static_cast<float>(screen.x) - overheadX) * m_ratio.x;
        auto maxH = (static_cast<float>(screen.y) - overheadY) * m_ratio.y;
        if (imgWidth > maxW || imgHeight > maxH)
        {
            scale = std::min(maxW / imgWidth, maxH / imgHeight);
        }
    }

    auto imgw = imgWidth * scale + tickness;
    auto imgh = imgHeight * scale + tickness;

    auto width = std::max<int>(imgw / m_ratio.x + overheadX, DefaultWindowSize.w);
    auto height = std::max<int>(imgh / m_ratio.y + overheadY, DefaultWindowSize.h);

    width = std::min<int>(width, screen.x);
    height = std::min<int>(height, screen.y);

    m_config.windowSize = { width, height };
    m_window.setSize({ width, height });

    if (m_config.centerWindow)
    {
        auto x = (screen.x - width) / 2;
        auto y = (screen.y - height) / 2;
        m_config.windowPos = { x, y };
        m_window.setPosition({ x, y });
    }
    else
    {
        // Keep current position but clamp so the window stays on screen.
        auto x = m_config.windowPos.x;
        auto y = m_config.windowPos.y;
        x = std::max(0, std::min(x, screen.x - width));
        y = std::max(0, std::min(y, screen.y - height));
        if (x != m_config.windowPos.x || y != m_config.windowPos.y)
        {
            m_config.windowPos = { x, y };
            m_window.setPosition({ x, y });
        }
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
    if (path == nullptr)
    {
        return;
    }

    m_config.fitImage = m_config.keepScale == false && m_config.fitImage;

    m_subImageForced = false;
    m_animation = false;
    m_imageInfo = {};
    m_image->reset();
    m_preview.reset();
    m_previewData = {};

    m_loader->loadImage(path);
    updateInfobar();
}

void cViewer::loadSubImage(int subStep)
{
    assert(subStep == -1 || subStep == 1);

    m_animation = false;
    m_imageInfo = {};
    m_image->reset();

    const auto& subInfo = m_loader->getImageInfo();
    const unsigned next = (subInfo.current + subInfo.images + subStep) % subInfo.images;
    if (subInfo.current != next)
    {
        m_loader->loadSubImage(next);
    }
}

void cViewer::updateInfobar()
{
    const auto* path = m_filesList->getName();

    m_infoBar->setFileName(path);
    m_infoBar->setScale(m_scale.getScale());
    m_infoBar->setFileIndex(m_filesList->getIndex(), m_filesList->getCount());

    if (m_loader->isLoaded())
    {
        const auto& chunk = m_loader->getChunkData();
        const auto& info = m_loader->getImageInfo();
        m_infoBar->setFormat(m_loader->getImageType());
        m_infoBar->setDimensions(chunk.width, chunk.height, info.bppImage);
        m_infoBar->setSubImage(info.current, info.images);
        m_infoBar->setMemory(info.fileSize, chunk.bitmap.size() + m_image->getGpuMemory());
    }
    else if (m_imageInfo.formatName != nullptr)
    {
        m_infoBar->setFormat(m_imageInfo.formatName);
        m_infoBar->setDimensions(m_imageInfo.width, m_imageInfo.height, m_imageInfo.bpp);
        m_infoBar->setSubImage(0, 0);
        m_infoBar->setMemory(m_imageInfo.size, 0);
    }
    else
    {
        m_infoBar->setFormat("unknown");
        m_infoBar->setDimensions(0, 0, 0);
        m_infoBar->setSubImage(0, 0);
        m_infoBar->setMemory(0, 0);
    }

    // Set window title to current filename
    if (path != nullptr)
    {
        const char* name = path;
        const char* p = std::strrchr(path, '/');
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
    return pos + getAdjustedCamera() - size * 0.5f;
}

void cViewer::updatePixelInfo(const Vectorf& pos)
{
    sPixelInfo pixelInfo;

    const Vectorf point = screenToImage(pos);

    pixelInfo.mouse = pos * m_scale.getScale();
    pixelInfo.point = point;

    if (m_loader->isLoaded())
    {
        const auto& chunk = m_loader->getChunkData();
        pixelInfo.bpp = chunk.bpp;

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
    const auto& startInfo = m_loader->getImageInfo();
    if (startInfo.isAnimation == false)
    {
        m_progress->show();
    }
    m_loadProgress.store(-2.0f, std::memory_order_relaxed); // "loading..."
    m_uploadActive.store(false, std::memory_order_relaxed);
    m_previewReady.store(false, std::memory_order_relaxed);
    // Note: m_preview and m_previewData are NOT cleaned here because
    // startLoading() runs on the loader thread. GL resources (m_preview)
    // and data read by the render thread (m_previewData) must only be
    // freed on the main thread — see upload-complete in onUpdate().
    m_imageInfoReady.store(false, std::memory_order_relaxed);
    m_bitmapAllocated.store(false, std::memory_order_relaxed);
    m_imagePrepared.store(false, std::memory_order_relaxed);
    m_uploadFinal = false;
}

void cViewer::onImageInfo(const sChunkData& chunk, const sImageInfo& info)
{
    m_imageInfo.width = chunk.width;
    m_imageInfo.height = chunk.height;
    m_imageInfo.bpp = info.bppImage;
    m_imageInfo.size = info.fileSize;
    m_imageInfo.formatName = info.formatName;
    m_imageInfoReady.store(true, std::memory_order_release);
}

void cViewer::onPreviewReady(sPreviewData&& preview)
{
    m_previewData = std::move(preview);
    m_previewReady.store(true, std::memory_order_release);
}

void cViewer::onBitmapAllocated(const sChunkData& /*chunk*/)
{
    m_loadProgress.store(-3.0f, std::memory_order_relaxed); // "decoding..."
    m_bitmapAllocated.store(true, std::memory_order_release);
}

void cViewer::doProgress(float progress)
{
    // Once upload is active, upload progress is the authoritative metric.
    if (m_uploadActive.load(std::memory_order_relaxed) == false)
    {
        m_loadProgress.store(progress * 0.5f, std::memory_order_relaxed);
    }
}

void cViewer::endLoading()
{
    m_uploadFinal = true;
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

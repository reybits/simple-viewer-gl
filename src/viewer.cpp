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
#include "deletionmark.h"
#include "fileslist.h"
#include "imageborder.h"
#include "imagegrid.h"
#include "imageloader.h"
#include "popups/exifpopup.h"
#include "popups/helppopup.h"
#include "popups/infobar.h"
#include "popups/pixelpopup.h"
#include "progress.h"
#include "quadimage.h"
#include "selection.h"

#include "types/vector.h"
#include <cassert>
#include <cmath>
#include <cstdlib>

namespace
{
    bool FixeScale(int& scale, int step)
    {
        const int oldScale = scale;
        scale /= step;
        scale *= step;
        return oldScale != scale;
    }

} // namespace

cViewer::cViewer(sConfig& config)
    : m_config(config)
    , m_isWindowed(true)
    , m_mouseLB(false)
    , m_mouseMB(false)
    , m_mouseRB(false)
    , m_angle(0)
{
    m_image.reset(new cQuadImage());
    m_loader.reset(new cImageLoader(&config, this));
    m_checkerBoard.reset(new cCheckerboard(config));
    m_deletionMark.reset(new cDeletionMark());
    m_infoBar.reset(new cInfoBar(config));
    m_pixelPopup.reset(new cPixelPopup());
    m_exifPopup.reset(new cExifPopup());
    m_helpPopup.reset(new cHelpPopup());
    m_progress.reset(new cProgress());
    m_border.reset(new cImageBorder());
    m_grid.reset(new cImageGrid());
    m_selection.reset(new cSelection());
    m_filesList.reset(new cFilesList(config.skipFilter, config.recursiveScan));
}

cViewer::~cViewer()
{
    m_image->clear();

    m_imgui.shutdown();
    cRenderer::shutdown();
}

void cViewer::setWindow(GLFWwindow* window)
{
    m_windowModeChangeRequested = false;

    cRenderer::init(window, 2048);
    m_imgui.init(window);

    m_checkerBoard->init();
    m_deletionMark->init();
    // m_infoBar->init();
    m_pixelPopup->init();
    // m_exifPopup->init();
    // m_helpPopup->init();
    m_progress->init();
    m_selection->init();

    Vectori fbSize;
    glfwGetFramebufferSize(window, &fbSize.x, &fbSize.y);
    Vectori winSize;
    glfwGetWindowSize(window, &winSize.x, &winSize.y);
    onResize(winSize, fbSize);
}

void cViewer::addPaths(const StringsList& paths)
{
    if (paths.size() != 0)
    {
        for (auto& path : paths)
        {
            m_filesList->addFile(path.c_str());
            // ::printf("path added: %s\n", paths[i]);
        }

        m_filesList->sortList();
        m_filesList->locateFile(paths[0].c_str());

        loadImage(0);
    }
}

void cViewer::onRender()
{
    cRenderer::beginFrame();
    m_imgui.beginFrame();

    m_checkerBoard->render();

    const float scale = m_scale.getScale();

    cRenderer::setGlobals(m_camera, m_angle, scale);

    m_image->render();

    const float half_w = ::roundf(m_image->getWidth() * 0.5f);
    const float half_h = ::roundf(m_image->getHeight() * 0.5f);

    if (m_loader->isLoaded())
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
    cRenderer::resetGlobals();

    if (m_config.showExif)
    {
        m_exifPopup->render();
    }

    if (m_config.hideInfobar == false)
    {
        m_infoBar->render();
    }

    if (m_filesList->isMarkedForDeletion())
    {
        m_deletionMark->render();
    }

    if (m_config.showPixelInfo && m_cursorInside && m_angle == 0)
    {
        m_pixelPopup->render();
    }

    m_helpPopup->render();

    m_progress->render();

    m_imgui.endFrame();
    cRenderer::endFrame();

    glfwSwapBuffers(cRenderer::getWindow());
}

void cViewer::onUpdate()
{
    if (m_imagePrepared == true)
    {
        m_imagePrepared = false;

        auto& desc = m_loader->getDescription();
        m_image->setBuffer(desc.width, desc.height, desc.pitch, desc.format, desc.bpp, desc.bitmap.data());

        if (m_loader->getMode() == cImageLoader::Mode::Image)
        {
            if (m_config.keepScale == false)
            {
                m_scale.setScalePercent(100);
                m_angle = 0;
                m_camera = { 0.0f, 0.0f };
            }

            m_selection->setImageDimension(desc.width, desc.height);
            m_exifPopup->setExifList(desc.exifList);
            centerWindow();
            enablePixelInfo(m_config.showPixelInfo);
        }

        updateInfobar();
    }

    if (isUploading())
    {
        const bool isDone = m_image->upload(m_config.mipmapTextureSize);
        m_progress->setProgress(0.5f + m_image->getProgress() * 0.5f);

        if (isDone)
        {
            m_progress->hide();

            auto& desc = m_loader->getDescription();
            m_animation = desc.isAnimation;
        }
    }
    else if (m_animation && m_subImageForced == false)
    {
        auto& desc = m_loader->getDescription();
        if (m_animationTime + desc.delay * 0.001f <= glfwGetTime())
        {
            m_animation = false;
            m_animationTime = glfwGetTime();
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
    // ::printf("(II) win size %d x %d\n", winSize.x, winSize.y);
    // ::printf("(II) fb size %d x %d\n\n", fbSize.x, fbSize.y);

    m_ratio = { (float)fbSize.x / winSize.x, (float)fbSize.y / winSize.y };
    // ::printf("(II) fb size %g x %g\n\n", m_ratio.x, m_ratio.y);

    auto window = cRenderer::getWindow();

    if (m_isWindowed)
    {
        m_config.windowSize = winSize;

        if (helpers::getPlatform() != helpers::Platform::Wayland)
        {
            glfwGetWindowPos(window, &m_config.windowPos.x, &m_config.windowPos.y);
        }
    }

    cRenderer::setViewportSize(fbSize);

    // const float scale = m_ratio.x * m_config.fontRatio;
    // m_imgui.setScale(scale);
    // m_pixelPopup->setScale(scale);
    // m_exifPopup->setScale(scale);
    // m_helpPopup->setScale(scale);
    // m_infoBar->setScale(scale);

    updatePixelInfo(m_lastMouse);
    updateInfobar();
}

void cViewer::onWindowResize(const Vectori& winSize)
{
    auto window = cRenderer::getWindow();

    Vectori fbSize;
    glfwGetFramebufferSize(window, &fbSize.x, &fbSize.y);

    onResize(winSize, fbSize);
}

void cViewer::onFramebufferResize(const Vectori& fbSize)
{
    auto window = cRenderer::getWindow();

    Vectori winSize;
    glfwGetWindowSize(window, &winSize.x, &winSize.y);

    onResize(winSize, fbSize);
}

void cViewer::onPosition(const Vectori& pos)
{
    m_config.windowPos = pos;
}

Vectorf cViewer::calculateMousePosition(const Vectorf& pos) const
{
    return pos * m_ratio / m_scale.getScale();
}

void cViewer::onMouse(const Vectorf& pos)
{
    m_imgui.onMousePosition(pos);

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

    showCursor(m_config.showPixelInfo == false);
    if (m_config.showPixelInfo)
    {
        const int cursor = m_selection->getCursor();
        m_pixelPopup->setCursor(cursor);

        const Vectorf point = screenToImage(posFixed);
        m_selection->mouseMove(point, m_scale.getScale());

        updatePixelInfo(posFixed);
    }
}

void cViewer::onCursorEnter(bool entered)
{
    m_cursorInside = entered;
}

void cViewer::onMouseScroll(const Vectorf& offset)
{
    m_imgui.onScroll(offset);

    if (m_config.wheelZoom)
    {
        updateScale(offset.y > 0.0f);
    }
    else
    {
        auto panRatio = m_ratio * m_config.panRatio;
        shiftCamera({ -offset.x * panRatio.x, -offset.y * panRatio.y });
    }
}

void cViewer::onMouseButtons(int button, int action, int /*mods*/)
{
    m_imgui.onMouseButton(button, action);

    updateMousePosition();

    switch (button)
    {
    case GLFW_MOUSE_BUTTON_LEFT:
        m_mouseLB = (action == GLFW_PRESS);
        {
            const Vectorf point = screenToImage(m_lastMouse);
            m_selection->mouseButton(point, m_scale.getScale(), m_mouseLB);

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

void cViewer::onKey(int key, int scancode, int action, int mods)
{
    m_imgui.onKey(key, scancode, action);

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
        glfwSetWindowShouldClose(cRenderer::getWindow(), 1);
        break;

    case GLFW_KEY_I:
        m_config.hideInfobar = !m_config.hideInfobar;
        // calculateScale();
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
            m_fitImage = m_config.fitImage;
            if (m_fitImage == false)
            {
                m_scale.setScalePercent(100);
            }
            m_camera = Vectorf();
            centerWindow();
            updateInfobar();
        }
        break;

    case GLFW_KEY_SPACE:
        loadImage(1);
        break;

    case GLFW_KEY_BACKSPACE:
        loadImage(-1);
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
                loadImage(0);
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
        updateScale(true);
        break;

    case GLFW_KEY_MINUS:
    case GLFW_KEY_KP_SUBTRACT:
        updateScale(false);
        break;

    case GLFW_KEY_C:
        m_config.backgroundIndex = (m_config.backgroundIndex + 1) % 5;
        break;

    case GLFW_KEY_ENTER:
    case GLFW_KEY_KP_ENTER:
        m_windowModeChangeRequested = true;
        break;

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
        showCursor(m_angle == 0 ? !m_config.showPixelInfo : true);
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
            m_scale.setScalePercent(1000);
            m_camera = { 0.0f, 0.0f };
            m_fitImage = false;
            centerWindow();
            updateInfobar();
        }
        else if (key >= GLFW_KEY_1 && key <= GLFW_KEY_9)
        {
            m_scale.setScalePercent((key - GLFW_KEY_0) * 100);
            m_camera = { 0.0f, 0.0f };
            m_fitImage = false;
            centerWindow();
            updateInfobar();
        }
        break;
    }
}

void cViewer::onChar(uint32_t c)
{
    m_imgui.onChar(c);
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
    const auto& viewport = cRenderer::getViewportSize();
    const auto half = Vectorf(viewport.x * inv + m_image->getWidth(), viewport.y * inv + m_image->getHeight()) * 0.5f;
    m_camera.x = std::max<float>(m_camera.x, -half.x);
    m_camera.x = std::min<float>(m_camera.x, half.x);
    m_camera.y = std::max<float>(m_camera.y, -half.y);
    m_camera.y = std::min<float>(m_camera.y, half.y);
}

void cViewer::calculateScale()
{
    if (m_fitImage && m_loader->isLoaded())
    {
        float w = static_cast<float>(m_image->getWidth());
        float h = static_cast<float>(m_image->getHeight());
        if (m_angle == 90 || m_angle == 270)
        {
            std::swap(w, h);
        }

        // scale only large images
        const auto& viewport = cRenderer::getViewportSize();
        if (w >= viewport.x || h >= viewport.y)
        {
            float aspect = w / h;
            float new_w = 0.0f;
            float new_h = 0.0f;
            float dx = w / viewport.x;
            float dy = h / viewport.y;
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
                // m_scale = static_cast<float>((angle == 0 || angle == 180) ? new_w : new_h) / w;
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

// TODO update m_camera_x / m_camera_y according current mouse position
void cViewer::updateScale(bool up)
{
    m_fitImage = false;

    int scale = m_scale.getScalePercent();

    if (up == true)
    {
        const int step = scale >= 100 ? 25 : (scale >= 50 ? 10 : (scale >= 30 ? 5 : 1));
        FixeScale(scale, step);
        scale += step;
    }
    else
    {
        const int step = scale > 100 ? 25 : (scale > 50 ? 10 : (scale > 30 ? 5 : 1));
        if (FixeScale(scale, step) == false && scale > step)
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
    auto window = cRenderer::getWindow();

    if (m_isWindowed)
    {
        if (m_config.centerWindow)
        {
            // calculate window size
            int imgw = m_image->getWidth() + (m_config.showImageBorder ? m_border->getThickness() * 2 : 0);
            int imgh = m_image->getHeight() + (m_config.showImageBorder ? m_border->getThickness() * 2 : 0);
            imgw = std::max<int>(imgw / m_ratio.x, std::max<int>(m_config.windowSize.w, DefaultWindowSize.w));
            imgh = std::max<int>(imgh / m_ratio.y, std::max<int>(m_config.windowSize.h, DefaultWindowSize.h));

            auto monitor = glfwGetPrimaryMonitor();
            auto mode = glfwGetVideoMode(monitor);

            const int width = m_fitImage
                ? m_config.windowSize.w
                : std::min<int>(imgw, mode->width);
            const int height = m_fitImage
                ? m_config.windowSize.h
                : std::min<int>(imgh, mode->height);

            // calculate window position
            const int x = (mode->width - width) / 2;
            const int y = (mode->height - height) / 2;

            if (helpers::getPlatform() != helpers::Platform::Wayland)
            {
                glfwSetWindowSize(window, width, height);
                glfwSetWindowPos(window, x, y);
            }

            m_config.windowSize = { width, height };
            m_config.windowPos = { x, y };
        }

        calculateScale();
    }
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

void cViewer::loadImage(int step)
{
    auto path = m_filesList->getName(step);
    loadImage(path);
}

void cViewer::loadImage(const char* path)
{
    m_fitImage = m_config.keepScale
        ? false
        : m_config.fitImage;

    m_subImageForced = false;
    m_animation = false;
    m_image->stop();

    m_loader->loadImage(path);
}

void cViewer::loadSubImage(int subStep)
{
    assert(subStep == -1 || subStep == 1);

    m_animation = false;
    m_image->stop();

    auto& desc = m_loader->getDescription();
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
        auto& desc = m_loader->getDescription();
        s.width = desc.width;
        s.height = desc.height;
        s.bpp = desc.bppImage;
        s.images = desc.images;
        s.current = desc.current;
        s.file_size = desc.size;
        s.mem_size = desc.bitmap.size();
        s.type = m_loader->getImageType();
    }
    else
    {
        s.type = "unknown";
    }
    m_infoBar->setInfo(s);
}

Vectorf cViewer::screenToImage(const Vectorf& pos) const
{
    const auto& viewport = cRenderer::getViewportSize();
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
        auto& desc = m_loader->getDescription();
        pixelInfo.bpp = desc.bpp;

        const int x = (int)point.x;
        const int y = (int)point.y;

        if (x >= 0 && y >= 0 && (unsigned)x <= m_image->getWidth() && (unsigned)y <= m_image->getHeight())
        {
            const auto idx = (size_t)(x * desc.bpp / 8 + y * desc.pitch);
            const auto color = desc.bitmap.data() + idx;

            if (desc.bpp == 24 || desc.bpp == 32)
            {
                const bool bgrx = desc.format == GL_BGRA || desc.format == GL_BGR;
                pixelInfo.color = {
                    color[bgrx ? 2 : 0],
                    color[1],
                    color[bgrx ? 0 : 2],
                    desc.bpp == 32 ? color[3] : (uint8_t)255
                };
            }
            else if (desc.bpp == 16)
            {
                const uint16_t c = ((uint16_t)color[1] << 8) | (uint16_t)color[0];
                if (desc.format == GL_UNSIGNED_SHORT_5_6_5)
                {
                    const float norm5 = 255.0f / 0x1f;
                    const float norm6 = 255.0f / 0x3f;
                    pixelInfo.color = {
                        (uint8_t)(((c >> 11) & 0x1f) * norm5),
                        (uint8_t)(((c >> 5) & 0x3f) * norm6),
                        (uint8_t)(((c >> 0) & 0x1f) * norm5),
                        255
                    };
                }
                else if (desc.format == GL_UNSIGNED_SHORT_4_4_4_4)
                {
                    const float norm4 = 255.0f / 0x0f;
                    pixelInfo.color = {
                        (uint8_t)(((c >> 12) & 0x0f) * norm4),
                        (uint8_t)(((c >> 8) & 0x0f) * norm4),
                        (uint8_t)(((c >> 4) & 0x0f) * norm4),
                        (uint8_t)(((c >> 0) & 0x0f) * norm4),
                    };
                }
                else if (desc.format == GL_UNSIGNED_SHORT_5_5_5_1)
                {
                    const float norm5 = 255.0f / 0x1f;
                    pixelInfo.color = {
                        (uint8_t)(((c >> 11) & 0x1f) * norm5),
                        (uint8_t)(((c >> 5) & 0x3f) * norm5),
                        (uint8_t)(((c >> 0) & 0x1f) * norm5),
                        (uint8_t)(((c >> 15) & 0x01) * 255)
                    };
                }
                else if (desc.format == GL_LUMINANCE_ALPHA)
                {
                    const uint8_t c = color[0];
                    const uint8_t a = color[1];
                    pixelInfo.color = { c, c, c, a };
                }
                else
                {
                    ::printf("(EE) Not implemented 16 bpp format: 0x%x\n", desc.format);
                }
            }
            else if (desc.bpp == 8)
            {
                const uint8_t c = color[0];
                pixelInfo.color = { c, c, c, 255 };
            }
        }

        pixelInfo.imgWidth = m_image->getWidth();
        pixelInfo.imgHeight = m_image->getHeight();
        pixelInfo.rc = m_selection->getRect();
    }

    m_pixelPopup->setPixelInfo(pixelInfo);
}

void cViewer::showCursor(bool show)
{
    auto window = cRenderer::getWindow();

    double x, y;
    glfwGetCursorPos(window, &x, &y);

    auto& size = m_config.windowSize;
    // ::printf("cursor %g x %g , window: %d x %d\n", pos.x, pos.y, size.w, size.h);
    m_cursorInside = !(x < 0.0 || x >= size.w || y < 0.0 || y >= size.h);

    glfwSetInputMode(window, GLFW_CURSOR, show ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
}

void cViewer::startLoading()
{
    auto& desc = m_loader->getDescription();
    if (desc.isAnimation == false)
    {
        m_progress->show();
    }
    m_imagePrepared = false;
}

void cViewer::doProgress(float progress)
{
    m_progress->setProgress(progress * 0.5f);
}

void cViewer::endLoading()
{
    m_imagePrepared = true;
}

void cViewer::updateMousePosition()
{
    GLFWwindow* window = cRenderer::getWindow();

    double x, y;
    glfwGetCursorPos(window, &x, &y);
    m_lastMouse = calculateMousePosition({ (float)x, (float)y });
}

void cViewer::enablePixelInfo(bool show)
{
    if (show)
    {
        updateMousePosition();
        updatePixelInfo(m_lastMouse);
    }
    m_config.showPixelInfo = show;
    showCursor(!show);
}

/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#pragma once

#include "common/callbacks.h"
#include "common/scale.h"
#include "gui.h"
#include "types/types.h"
#include "types/vector.h"

#include <memory>

class cCheckerboard;
class cDeletionMark;
class cExifPopup;
class cFilesList;
class cHelpPopup;
class cImageBorder;
class cImageGrid;
class cImageLoader;
class cInfoBar;
class cPixelPopup;
class cProgress;
class cQuadImage;
class cSelection;
struct GLFWwindow;
struct sConfig;

class cViewer final : public iCallbacks
{
public:
    explicit cViewer(sConfig& config);
    ~cViewer();

    void setWindow(GLFWwindow* window);

    void addPaths(const StringsList& paths);

    bool isUploading() const;

    bool isWindowModeRequested() const
    {
        return m_windowModeChangeRequested;
    }

    bool isWindowed() const
    {
        return m_isWindowed;
    }
    void setWindowed(bool windowed)
    {
        m_isWindowed = windowed;
    }

public:
    virtual void startLoading() override;
    virtual void doProgress(float progress) override;
    virtual void endLoading() override;

    void onRender();
    void onUpdate();
    void onWindowResize(const Vectori& winSize);
    void onFramebufferResize(const Vectori& fbSize);
    void onPosition(const Vectori& pos);
    void onMouse(const Vectorf& pos);
    void onCursorEnter(bool entered);
    void onMouseScroll(const Vectorf& offset);
    void onMouseButtons(int button, int action, int mods);
    void onKey(int key, int scancode, int action, int mods);
    void onChar(uint32_t c);

    void centerWindow();
    void showCursor(bool show);

private:
    void onResize(const Vectori& winSize, const Vectori& fbSize);
    void loadFirstImage();
    void loadLastImage();
    void loadImage(int step);
    void loadImage(const char* path);
    void loadSubImage(int subStep);
    void calculateScale();
    enum class ScaleDirection
    {
        Up,
        Down,
    };
    void updateScale(ScaleDirection direction);
    void updateFiltering();
    void updateInfobar();
    void updatePixelInfo(const Vectorf& pos);

    float getStepVert(bool byPixel) const;
    float getStepHori(bool byPixel) const;
    void keyUp(bool byPixel);
    void keyDown(bool byPixel);
    void keyLeft(bool byPixel);
    void keyRight(bool byPixel);
    void shiftCamera(const Vectorf& delta);
    Vectorf screenToImage(const Vectorf& pos) const;
    Vectorf calculateMousePosition(const Vectorf& pos) const;
    void updateMousePosition();
    void enablePixelInfo(bool show);

private:
    sConfig& m_config;

    Vectorf m_ratio;
    bool m_imagePrepared = false;
    cScale m_scale;
    bool m_isWindowed;
    bool m_cursorInside = false;
    bool m_windowModeChangeRequested = false;
    bool m_mouseLB, m_mouseMB, m_mouseRB;
    Vectorf m_lastMouse;
    Vectorf m_camera;
    int m_angle;

    bool m_fitImage = false;
    bool m_subImageForced = false;
    bool m_animation = false;
    float m_animationTime = 0.0f;

    cGui m_imgui;

    std::unique_ptr<cQuadImage> m_image;
    std::unique_ptr<cFilesList> m_filesList;
    std::unique_ptr<cProgress> m_progress;
    std::unique_ptr<cImageLoader> m_loader;
    std::unique_ptr<cInfoBar> m_infoBar;
    std::unique_ptr<cPixelPopup> m_pixelPopup;
    std::unique_ptr<cExifPopup> m_exifPopup;
    std::unique_ptr<cHelpPopup> m_helpPopup;
    std::unique_ptr<cCheckerboard> m_checkerBoard;
    std::unique_ptr<cDeletionMark> m_deletionMark;
    std::unique_ptr<cImageBorder> m_border;
    std::unique_ptr<cImageGrid> m_grid;
    std::unique_ptr<cSelection> m_selection;
};

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
#include "window.h"

#include <atomic>
#include <memory>

class cCheckerboard;
class cDeletionMark;
class cExifPopup;
class cFileBrowser;
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
struct sConfig;

class cViewer final : public iWindowEvents, public iCallbacks
{
public:
    cViewer(sConfig& config, cWindow& window);
    ~cViewer();

    void addPaths(const StringsList& paths);

    bool isUploading() const;

    void onRender();
    void onUpdate();

    // iWindowEvents
    void onWindowResize(const Vectori& winSize) override;
    void onFramebufferResize(const Vectori& fbSize) override;
    void onWindowPosition(const Vectori& pos) override;
    void onWindowRefresh() override;
    void onKeyEvent(int key, int scancode, int action, int mods) override;
    void onCharEvent(uint32_t codepoint) override;
    void onMouseButton(int button, int action, int mods) override;
    void onMouseMove(const Vectorf& pos) override;
    void onMouseScroll(const Vectorf& offset) override;
    void onFileDrop(const StringsList& paths) override;

    // iCallbacks
    void startLoading() override;
    void onBitmapAllocated(const sBitmapDescription& desc) override;
    void doProgress(float progress) override;
    void endLoading() override;

    // Called by cWindow after fullscreen toggle to reinit GL resources
    void onContextRecreated();

    void centerWindow();

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
    void showCursor(bool show);
    void enablePixelInfo(bool show);

private:
    sConfig& m_config;
    cWindow& m_window;

    Vectorf m_ratio;
    std::atomic<bool> m_bitmapAllocated{ false };
    std::atomic<bool> m_imagePrepared{ false };
    std::atomic<float> m_loadProgress{ -1.0f };
    bool m_uploadFinal = false;
    cScale m_scale;
    bool m_cursorInside = false;
    bool m_mouseLB = false;
    bool m_mouseMB = false;
    bool m_mouseRB = false;
    Vectorf m_lastMouse;
    Vectorf m_camera;
    int m_angle = 0;

    bool m_subImageForced = false;
    bool m_animation = false;
    float m_animationTime = 0.0f;

    cGui m_imgui;

    std::unique_ptr<cQuadImage> m_image;
    std::unique_ptr<cFilesList> m_filesList;
    std::unique_ptr<cFileBrowser> m_fileSelector;
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

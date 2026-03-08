/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#pragma once

#include "Common/Callbacks.h"
#include "Common/Scale.h"
#include "Gui.h"
#include "Types/Types.h"
#include "Types/Vector.h"
#include "Window.h"

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

class cViewer final
{
public:
    cViewer(sConfig& config, cWindow& window);
    ~cViewer();

    void addPaths(const StringsList& paths);

    bool isUploading() const;

    void onRender();
    void onUpdate();

    sWindowEvents& getWindowEvents()
    {
        return m_windowEvents;
    }

private:
    // Window event handlers
    void onWindowResize(const Vectori& winSize);
    void onFramebufferResize(const Vectori& fbSize);
    void onWindowPosition(const Vectori& pos);
    void onWindowRefresh();
    void onKeyEvent(int key, int scancode, int action, int mods);
    void onCharEvent(uint32_t codepoint);
    void onMouseButton(int button, int action, int mods);
    void onMouseMove(const Vectorf& pos);
    void onMouseScroll(const Vectorf& offset);
    void onFileDrop(const StringsList& paths);

    // Loader callback handlers
    void startLoading();
    void onBitmapAllocated(const sBitmapDescription& desc);
    void doProgress(float progress);
    void endLoading();

    void onContextRecreated();
    void onResize(const Vectori& winSize, const Vectori& fbSize);
    void centerWindow();
    void resetViewAndUpdate(int scalePercent);
    void loadFirstImage();
    void loadLastImage();
    void navigateImage(int step);
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
    void updateCursorState(bool visible);
    void enablePixelInfo(bool show);

private:
    sConfig& m_config;
    cWindow& m_window;
    sWindowEvents m_windowEvents;
    sCallbacks m_callbacks;

    Vectorf m_ratio;
    std::atomic<bool> m_bitmapAllocated{ false };
    std::atomic<bool> m_imagePrepared{ false };
    std::atomic<float> m_loadProgress{ -1.0f };
    bool m_uploadFinal = false;
    cScale m_scale;
    bool m_cursorInside = false;
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

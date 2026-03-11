/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "Types/Vector.h"

#include <cstdint>

class cWindow;
struct sConfig;

class cGui final
{
public:
    cGui(cWindow& window, const sConfig& config);
    ~cGui();

    void beginFrame();
    void endFrame();

    void setFps(float fps);
    void setInfoBarVisible(bool visible);

    // Infobar height in window/logical coords (font + padding).
    float getInfoBarHeight() const;

    // Central area available for image rendering (in window/logical coords).
    // Equals the viewport minus the infobar (when visible) minus any docked windows.
    const Vectorf& getCentralPos() const
    {
        return m_centralPos;
    }
    const Vectorf& getCentralSize() const
    {
        return m_centralSize;
    }

    // Returns true when ImGui wants to capture input (e.g., typing in a text field,
    // interacting with a focused window). Viewer should skip its own input handling.
    bool wantCaptureKeyboard() const;
    bool wantCaptureMouse() const;

public:
    void onMousePosition(const Vectorf& pos);
    void onMouseButton(int button, int action);
    void onScroll(const Vectorf& pos);
    void onKey(int key, int scancode, int action);
    void onChar(uint32_t c);

private:
    cWindow& m_window;
    const sConfig& m_config;

private:
    double m_time = 0.0f;
    float m_fps = 0.0f;
    bool m_infobarVisible = true;
    Vectorf m_centralPos;
    Vectorf m_centralSize;
};

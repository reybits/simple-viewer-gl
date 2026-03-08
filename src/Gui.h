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

class cGui
{
public:
    void init(cWindow& window);
    void shutdown();

    void beginFrame();
    void endFrame();

    // Central area of the DockSpace (in window/logical coords).
    // This is the remaining area after all docked windows are subtracted.
    const Vectorf& getCentralPos() const { return m_centralPos; }
    const Vectorf& getCentralSize() const { return m_centralSize; }

public:
    void onMousePosition(const Vectorf& pos);
    void onMouseButton(int button, int action);
    void onScroll(const Vectorf& pos);
    void onKey(int key, int scancode, int action);
    void onChar(uint32_t c);

private:
    cWindow* m_window = nullptr;
    double m_time = 0.0f;
    Vectorf m_centralPos;
    Vectorf m_centralSize;
};

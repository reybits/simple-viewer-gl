/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*  and@reybits.dev
*
\**********************************************/

#include "Types/Types.h"
#include "Types/Vector.h"

class cWindow;

class cGui
{
public:
    void init(cWindow& window);
    void shutdown();

    void beginFrame();
    void endFrame();

public:
    void onMousePosition(const Vectorf& pos);
    void onMouseButton(int button, int action);
    void onScroll(const Vectorf& pos);
    void onKey(int key, int scancode, int action);
    void onChar(uint32_t c);

private:
    cWindow* m_window = nullptr;
    double m_time = 0.0f;
};

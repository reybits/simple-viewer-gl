/**********************************************\
*
*  Simple Viewer GL edition
*  by Andrey A. Ugolnik
*  http://www.ugolnik.info
*  andrey@ugolnik.info
*
\**********************************************/

#include "types/types.h"
#include "types/vector.h"

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

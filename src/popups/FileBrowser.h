/**********************************************\
*
*  Simple Viewer GL
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*
\**********************************************/

#pragma once

#include <functional>
#include <imgui/imgui.h>
#include <memory>
#include <string>

#include "types/vector.h"

namespace ImGui
{
    class FileBrowser;

}

class cFileBrowser final
{
public:
    cFileBrowser();
    ~cFileBrowser();

    void setWindowSize(int width, int height);

    using FilterList = std::vector<std::string>;
    void setFilter(const FilterList& filterList);

    bool isVisible() const;

    struct Result
    {
        std::string directory;
        std::string fileName;
    };

    Result getResult() const;

    using OnClosed = std::function<void(const Result&)>;

    enum class Type
    {
        Open,
        Write
    };

    void open(OnClosed onClosed, Type type, const std::string& title, const std::string& dir = {});

    void render();

    void close();

private:
    std::unique_ptr<ImGui::FileBrowser> m_fileBrowser;

private:
    Result m_result;
    OnClosed m_onClosed;
    FilterList m_filterList;

    Vectori m_windowSize;
    Vectori m_windowPosition;
};

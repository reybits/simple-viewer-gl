/**********************************************\
*
*  Simple Viewer GL
*  by Andrey A. Ugolnik
*  https://github.com/reybits
*
\**********************************************/

#include "FileBrowser.h"

#include <cassert>
#include <filebrowser/imfilebrowser.h>

cFileBrowser::cFileBrowser()
{
}

cFileBrowser::~cFileBrowser()
{
}

void cFileBrowser::setWindowSize(int width, int height)
{
    m_windowSize = {
        static_cast<int>(width * 0.9f),
        static_cast<int>(height * 0.9f)
    };

    m_windowPosition = {
        (width - m_windowSize.x) / 2,
        (height - m_windowSize.y) / 2
    };

    if (m_fileBrowser.get() != nullptr)
    {
        m_fileBrowser->SetWindowPos(m_windowPosition.x, m_windowPosition.y);
        m_fileBrowser->SetWindowSize(m_windowSize.x, m_windowSize.y);
    }
}

void cFileBrowser::setFilter(const FilterList& filterList)
{
    m_filterList = filterList;
}

bool cFileBrowser::isVisible() const
{
    return m_fileBrowser.get() != nullptr
        ? m_fileBrowser->IsOpened()
        : false;
}

void cFileBrowser::open(OnClosed onClosed, Type type, const std::string& title, const std::string& dir)
{
    m_onClosed = onClosed;

    auto flags = ImGuiFileBrowserFlags_CloseOnEsc
        | ImGuiFileBrowserFlags_ConfirmOnEnter;
    if (type == Type::Write)
    {
        flags |= ImGuiFileBrowserFlags_EnterNewFilename
            | ImGuiFileBrowserFlags_CreateNewDir;
    }

    m_fileBrowser = std::make_unique<ImGui::FileBrowser>(flags);

    m_fileBrowser->SetWindowPos(m_windowPosition.x, m_windowPosition.y);
    m_fileBrowser->SetWindowSize(m_windowSize.x, m_windowSize.y);

    m_fileBrowser->SetTitle(title);
    m_fileBrowser->SetTypeFilters(m_filterList);

    if (dir.empty() == false)
    {
        m_fileBrowser->SetDirectory(dir);
    }
    else
    {
        m_fileBrowser->SetDirectory();
    }

    m_fileBrowser->Open();
}

void cFileBrowser::render()
{
    if (m_fileBrowser.get() != nullptr)
    {
        m_fileBrowser->Display();

        auto hasSelected = m_fileBrowser->HasSelected();
        if (hasSelected)
        {
            auto selected = m_fileBrowser->GetSelected();
            m_result = { selected.parent_path(), selected.filename() };

            assert(m_onClosed != nullptr);
            m_onClosed(m_result);

            close();
        }
    }
}

cFileBrowser::Result cFileBrowser::getResult() const
{
    return m_result;
}

void cFileBrowser::close()
{
    if (m_fileBrowser.get() != nullptr)
    {
        auto hasSelected = m_fileBrowser->HasSelected();
        if (hasSelected)
        {
            auto selected = m_fileBrowser->GetSelected();
            m_result = { selected.parent_path(), selected.filename() };
        }

        m_fileBrowser->ClearSelected();
        m_fileBrowser.reset();
    }
}

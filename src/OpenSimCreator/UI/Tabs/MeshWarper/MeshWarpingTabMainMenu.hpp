#pragma once

#include <OpenSimCreator/UI/Tabs/MeshWarper/MeshWarpingTabEditMenu.hpp>
#include <OpenSimCreator/UI/Tabs/MeshWarper/MeshWarpingTabSharedState.hpp>
#include <OpenSimCreator/UI/Tabs/MeshWarper/MeshWarpingTabFileMenu.hpp>
#include <OpenSimCreator/UI/Widgets/MainMenu.hpp>

#include <oscar/UI/Panels/PanelManager.hpp>
#include <oscar/UI/Widgets/WindowMenu.hpp>

#include <memory>

namespace osc
{
    // widget: the main menu (contains multiple submenus: 'file', 'edit', 'about', etc.)
    class MeshWarpingTabMainMenu final {
    public:
        explicit MeshWarpingTabMainMenu(
            std::shared_ptr<MeshWarpingTabSharedState> const& tabState_,
            std::shared_ptr<PanelManager> const& panelManager_) :
            m_FileMenu{tabState_},
            m_EditMenu{tabState_},
            m_WindowMenu{panelManager_}
        {
        }

        void onDraw()
        {
            m_FileMenu.onDraw();
            m_EditMenu.onDraw();
            m_WindowMenu.onDraw();
            m_AboutTab.onDraw();
        }
    private:
        MeshWarpingTabFileMenu m_FileMenu;
        MeshWarpingTabEditMenu m_EditMenu;
        WindowMenu m_WindowMenu;
        MainMenuAboutTab m_AboutTab;
    };
}

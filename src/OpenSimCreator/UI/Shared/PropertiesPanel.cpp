#include "PropertiesPanel.hpp"

#include <OpenSimCreator/Documents/Model/UndoableModelStatePair.hpp>
#include <OpenSimCreator/Documents/Model/UndoableModelActions.hpp>
#include <OpenSimCreator/UI/ModelEditor/ComponentContextMenu.hpp>
#include <OpenSimCreator/UI/ModelEditor/EditorAPI.hpp>
#include <OpenSimCreator/UI/ModelEditor/ReassignSocketPopup.hpp>
#include <OpenSimCreator/UI/ModelEditor/SelectComponentPopup.hpp>
#include <OpenSimCreator/UI/ModelEditor/SelectGeometryPopup.hpp>
#include <OpenSimCreator/UI/Shared/ObjectPropertiesEditor.hpp>
#include <OpenSimCreator/Utils/OpenSimHelpers.hpp>

#include <IconsFontAwesome5.h>
#include <imgui.h>
#include <OpenSim/Common/Component.h>
#include <OpenSim/Common/ComponentList.h>
#include <OpenSim/Common/ComponentSocket.h>
#include <OpenSim/Common/Object.h>
#include <OpenSim/Common/Property.h>
#include <OpenSim/Common/Set.h>
#include <OpenSim/Simulation/Model/ContactGeometry.h>
#include <OpenSim/Simulation/Model/Geometry.h>
#include <OpenSim/Simulation/Model/HuntCrossleyForce.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/Model/PathActuator.h>
#include <OpenSim/Simulation/Model/PhysicalFrame.h>
#include <OpenSim/Simulation/SimbodyEngine/Joint.h>
#include <oscar/Graphics/Color.hpp>
#include <oscar/Platform/os.hpp>
#include <oscar/UI/Panels/StandardPanelImpl.hpp>
#include <oscar/UI/ImGuiHelpers.hpp>
#include <oscar/Utils/ScopeGuard.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
    void DrawActionsMenu(osc::EditorAPI* editorAPI, std::shared_ptr<osc::UndoableModelStatePair> const& model)
    {
        OpenSim::Component const* selection = model->getSelected();
        if (!selection)
        {
            return;
        }

        ImGui::Columns(2);
        ImGui::TextUnformatted("actions");
        ImGui::SameLine();
        osc::DrawHelpMarker("Shows a menu containing extra actions that can be performed on this component.\n\nYou can also access the same menu by right-clicking the component in the 3D viewer, bottom status bar, or navigator panel.");
        ImGui::NextColumn();
        osc::PushStyleColor(ImGuiCol_Text, osc::Color::yellow());
        if (ImGui::Button(ICON_FA_BOLT) || ImGui::IsItemClicked(ImGuiMouseButton_Right))
        {
            editorAPI->pushComponentContextMenuPopup(osc::GetAbsolutePath(*selection));
        }
        osc::PopStyleColor();
        ImGui::NextColumn();
        ImGui::Columns();
    }

    class ObjectNameEditor final {
    public:
        explicit ObjectNameEditor(std::shared_ptr<osc::UndoableModelStatePair> model_) :
            m_Model{std::move(model_)}
        {
        }

        void onDraw()
        {
            OpenSim::Component const* const selected = m_Model->getSelected();

            if (!selected)
            {
                return;  // don't do anything if nothing is selected
            }

            // update cached edits if model/selection changes
            if (m_Model->getModelVersion() != m_LastModelVersion || selected != m_LastSelected)
            {
                m_EditedName = selected->getName();
                m_LastModelVersion = m_Model->getModelVersion();
                m_LastSelected = selected;
            }

            ImGui::Columns(2);

            ImGui::Separator();
            ImGui::TextUnformatted("name");
            ImGui::SameLine();
            osc::DrawHelpMarker("The name of the component", "The component's name can be important. It can be used when components want to refer to eachover. E.g. a joint will name the two frames it attaches to.");

            ImGui::NextColumn();

            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
            osc::InputString("##nameeditor", m_EditedName);
            if (osc::ItemValueShouldBeSaved())
            {
                osc::ActionSetComponentName(*m_Model, osc::GetAbsolutePath(*selected), m_EditedName);
            }

            ImGui::NextColumn();

            ImGui::Columns();
        }
    private:
        std::shared_ptr<osc::UndoableModelStatePair> m_Model;
        osc::UID m_LastModelVersion;
        OpenSim::Component const* m_LastSelected = nullptr;
        std::string m_EditedName;
    };
}

class osc::PropertiesPanel::Impl final : public osc::StandardPanelImpl {
public:
    Impl(
        std::string_view panelName,
        EditorAPI* editorAPI,
        std::shared_ptr<UndoableModelStatePair> model) :

        StandardPanelImpl{panelName},
        m_EditorAPI{editorAPI},
        m_Model{std::move(model)},
        m_SelectionPropertiesEditor{editorAPI, m_Model, [model = m_Model](){ return model->getSelected(); }}
    {
    }

private:
    void implDrawContent() final
    {
        if (!m_Model->getSelected())
        {
            ImGui::TextUnformatted("(nothing selected)");
            return;
        }

        ImGui::PushID(m_Model->getSelected());
        ScopeGuard const g{[]() { ImGui::PopID(); }};

        // draw an actions row with a button that opens the context menu
        //
        // it's helpful to reveal to users that actions are available (#426)
        DrawActionsMenu(m_EditorAPI, m_Model);

        m_NameEditor.onDraw();

        if (!m_Model->getSelected())
        {
            return;
        }

        // property editors
        {
            auto maybeUpdater = m_SelectionPropertiesEditor.onDraw();
            if (maybeUpdater)
            {
                osc::ActionApplyPropertyEdit(*m_Model, *maybeUpdater);
            }
        }
    }

    EditorAPI* m_EditorAPI;
    std::shared_ptr<UndoableModelStatePair> m_Model;
    ObjectNameEditor m_NameEditor{m_Model};
    ObjectPropertiesEditor m_SelectionPropertiesEditor;
};


// public API (PIMPL)

osc::PropertiesPanel::PropertiesPanel(
    std::string_view panelName,
    EditorAPI* editorAPI,
    std::shared_ptr<UndoableModelStatePair> model) :
    m_Impl{std::make_unique<Impl>(panelName, editorAPI, std::move(model))}
{
}

osc::PropertiesPanel::PropertiesPanel(PropertiesPanel&&) noexcept = default;
osc::PropertiesPanel& osc::PropertiesPanel::operator=(PropertiesPanel&&) noexcept = default;
osc::PropertiesPanel::~PropertiesPanel() noexcept = default;

osc::CStringView osc::PropertiesPanel::implGetName() const
{
    return m_Impl->getName();
}

bool osc::PropertiesPanel::implIsOpen() const
{
    return m_Impl->isOpen();
}

void osc::PropertiesPanel::implOpen()
{
    m_Impl->open();
}

void osc::PropertiesPanel::implClose()
{
    m_Impl->close();
}

void osc::PropertiesPanel::implOnDraw()
{
    m_Impl->onDraw();
}

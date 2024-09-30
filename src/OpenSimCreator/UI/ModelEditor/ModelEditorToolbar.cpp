#include "ModelEditorToolbar.h"

#include <OpenSimCreator/Documents/Model/Environment.h>
#include <OpenSimCreator/Documents/Model/UndoableModelActions.h>
#include <OpenSimCreator/Documents/Model/UndoableModelStatePair.h>
#include <OpenSimCreator/UI/ModelEditor/IEditorAPI.h>
#include <OpenSimCreator/UI/Shared/BasicWidgets.h>
#include <OpenSimCreator/UI/Shared/ParamBlockEditorPopup.h>

#include <oscar/Graphics/Color.h>
#include <oscar/Maths/Vec2.h>
#include <oscar/Platform/App.h>
#include <oscar/Platform/IconCodepoints.h>
#include <oscar/Platform/Widget.h>
#include <oscar/UI/Events/OpenPopupEvent.h>
#include <oscar/UI/IconCache.h>
#include <oscar/UI/oscimgui.h>
#include <oscar/Utils/LifetimedPtr.h>

#include <algorithm>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

class osc::ModelEditorToolbar::Impl final {
public:
    Impl(
        std::string_view label_,
        Widget& parent_,
        IEditorAPI* editorAPI_,
        std::shared_ptr<UndoableModelStatePair> model_) :

        m_Label{label_},
        m_Parent{parent_.weak_ref()},
        m_EditorAPI{editorAPI_},
        m_Model{std::move(model_)}
    {}

    void onDraw()
    {
        if (BeginToolbar(m_Label, Vec2{5.0f, 5.0f}))
        {
            drawContent();
        }
        ui::end_panel();
    }
private:
    void drawModelFileRelatedButtons()
    {
        DrawNewModelButton(*m_Parent);
        ui::same_line();
        DrawOpenModelButtonWithRecentFilesDropdown(*m_Parent);
        ui::same_line();
        DrawSaveModelButton(*m_Parent, *m_Model);
        ui::same_line();
        DrawReloadModelButton(*m_Model);
    }

    void drawForwardDynamicSimulationControls()
    {
        ui::push_style_var(ui::StyleVar::ItemSpacing, {2.0f, 0.0f});

        ui::push_style_color(ui::ColorVar::Text, Color::dark_green());
        if (ui::draw_button(OSC_ICON_PLAY))
        {
            ActionStartSimulatingModel(*m_Parent, *m_Model);
        }
        ui::pop_style_color();
        App::upd().add_frame_annotation("Simulate Button", ui::get_last_drawn_item_screen_rect());
        ui::draw_tooltip_if_item_hovered("Simulate Model", "Run a forward-dynamic simulation of the model");

        ui::same_line();

        if (ui::draw_button(OSC_ICON_EDIT))
        {
            auto popup = std::make_unique<ParamBlockEditorPopup>(
                "simulation parameters",
                &m_Model->tryUpdEnvironment()->updSimulationParams()
            );
            App::post_event<OpenPopupEvent>(*m_Parent, std::move(popup));
        }
        ui::draw_tooltip_if_item_hovered("Edit Simulation Settings", "Change the parameters used when simulating the model");

        ui::pop_style_var();
    }

    void drawContent()
    {
        drawModelFileRelatedButtons();
        ui::draw_same_line_with_vertical_separator();

        DrawUndoAndRedoButtons(*m_Model);
        ui::draw_same_line_with_vertical_separator();

        DrawSceneScaleFactorEditorControls(*m_Model);
        ui::draw_same_line_with_vertical_separator();

        drawForwardDynamicSimulationControls();
        ui::draw_same_line_with_vertical_separator();

        DrawAllDecorationToggleButtons(*m_Model, *m_IconCache);
    }

    std::string m_Label;
    LifetimedPtr<Widget> m_Parent;
    IEditorAPI* m_EditorAPI;
    std::shared_ptr<UndoableModelStatePair> m_Model;

    std::shared_ptr<IconCache> m_IconCache = App::singleton<IconCache>(
        App::resource_loader().with_prefix("icons/"),
        ui::get_text_line_height()/128.0f
    );
};


osc::ModelEditorToolbar::ModelEditorToolbar(
    std::string_view label_,
    Widget& parent_,
    IEditorAPI* editorAPI_,
    std::shared_ptr<UndoableModelStatePair> model_) :

    m_Impl{std::make_unique<Impl>(label_, parent_, editorAPI_, std::move(model_))}
{}
osc::ModelEditorToolbar::ModelEditorToolbar(ModelEditorToolbar&&) noexcept = default;
osc::ModelEditorToolbar& osc::ModelEditorToolbar::operator=(ModelEditorToolbar&&) noexcept = default;
osc::ModelEditorToolbar::~ModelEditorToolbar() noexcept = default;

void osc::ModelEditorToolbar::onDraw()
{
    m_Impl->onDraw();
}

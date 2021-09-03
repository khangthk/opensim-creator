#include "model_editor_screen.hpp"

#include "src/3d/gl.hpp"
#include "src/opensim_bindings/file_change_poller.hpp"
#include "src/opensim_bindings/opensim_helpers.hpp"
#include "src/opensim_bindings/type_registry.hpp"
#include "src/opensim_bindings/ui_types.hpp"
#include "src/screens/error_screen.hpp"
#include "src/screens/simulator_screen.hpp"
#include "src/ui/add_body_popup.hpp"
#include "src/ui/attach_geometry_popup.hpp"
#include "src/ui/component_details.hpp"
#include "src/ui/component_hierarchy.hpp"
#include "src/ui/fd_params_editor_popup.hpp"
#include "src/ui/help_marker.hpp"
#include "src/ui/main_menu.hpp"
#include "src/ui/model_actions.hpp"
#include "src/ui/log_viewer.hpp"
#include "src/ui/properties_editor.hpp"
#include "src/ui/reassign_socket_popup.hpp"
#include "src/ui/select_component_popup.hpp"
#include "src/ui/select_1_pf_popup.hpp"
#include "src/ui/select_2_pfs_popup.hpp"
#include "src/app.hpp"
#include "src/log.hpp"
#include "src/main_editor_state.hpp"
#include "src/os.hpp"
#include "src/styling.hpp"

#include <imgui.h>
#include <OpenSim/Common/Component.h>
#include <OpenSim/Common/Object.h>
#include <OpenSim/Common/Property.h>
#include <OpenSim/Common/Set.h>
#include <OpenSim/Simulation/Control/Controller.h>
#include <OpenSim/Simulation/Model/BodySet.h>
#include <OpenSim/Simulation/Model/ContactGeometrySet.h>
#include <OpenSim/Simulation/Model/ControllerSet.h>
#include <OpenSim/Simulation/Model/ConstraintSet.h>
#include <OpenSim/Simulation/Model/Frame.h>
#include <OpenSim/Simulation/Model/ForceSet.h>
#include <OpenSim/Simulation/Model/Geometry.h>
#include <OpenSim/Simulation/Model/HuntCrossleyForce.h>
#include <OpenSim/Simulation/Model/JointSet.h>
#include <OpenSim/Simulation/Model/MarkerSet.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <OpenSim/Simulation/Model/PathActuator.h>
#include <OpenSim/Simulation/Model/PathPoint.h>
#include <OpenSim/Simulation/Model/PathPointSet.h>
#include <OpenSim/Simulation/Model/PhysicalFrame.h>
#include <OpenSim/Simulation/Model/PhysicalOffsetFrame.h>
#include <OpenSim/Simulation/Model/ProbeSet.h>
#include <OpenSim/Simulation/SimbodyEngine/Joint.h>
#include <OpenSim/Simulation/Wrap/WrapObject.h>
#include <OpenSim/Simulation/Wrap/WrapObjectSet.h>
#include <SDL_keyboard.h>
#include <SimTKcommon.h>

#include <array>
#include <cstddef>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace osc;

namespace {

    // returns the first ancestor of `c` that has type `T`
    template<typename T>
    [[nodiscard]] T const* find_ancestor(OpenSim::Component const* c) {
        while (c) {
            T const* p = dynamic_cast<T const*>(c);
            if (p) {
                return p;
            }
            c = c->hasOwner() ? &c->getOwner() : nullptr;
        }
        return nullptr;
    }

    // returns true if the model has a backing file
    [[nodiscard]] bool has_backing_file(OpenSim::Model const& m) {
        return m.getInputFileName() != "Unassigned";
    }

    // copy common joint properties from a `src` to `dest`
    //
    // e.g. names, coordinate names, etc.
    void copy_common_joint_properties(OpenSim::Joint const& src, OpenSim::Joint& dest) {
        dest.setName(src.getName());

        // copy owned frames
        dest.updProperty_frames().assign(src.getProperty_frames());

        // copy, or reference, the parent based on whether the source owns it
        {
            OpenSim::PhysicalFrame const& src_parent = src.getParentFrame();
            bool parent_assigned = false;
            for (int i = 0; i < src.getProperty_frames().size(); ++i) {
                if (&src.get_frames(i) == &src_parent) {
                    // the source's parent is also owned by the source, so we need to
                    // ensure the destination refers to its own (cloned, above) copy
                    dest.connectSocket_parent_frame(dest.get_frames(i));
                    parent_assigned = true;
                    break;
                }
            }
            if (!parent_assigned) {
                // the source's parent is a reference to some frame that the source
                // doesn't, itself, own, so the destination should just also refer
                // to the same (not-owned) frame
                dest.connectSocket_parent_frame(src_parent);
            }
        }

        // copy, or reference, the child based on whether the source owns it
        {
            OpenSim::PhysicalFrame const& src_child = src.getChildFrame();
            bool child_assigned = false;
            for (int i = 0; i < src.getProperty_frames().size(); ++i) {
                if (&src.get_frames(i) == &src_child) {
                    // the source's child is also owned by the source, so we need to
                    // ensure the destination refers to its own (cloned, above) copy
                    dest.connectSocket_child_frame(dest.get_frames(i));
                    child_assigned = true;
                    break;
                }
            }
            if (!child_assigned) {
                // the source's child is a reference to some frame that the source
                // doesn't, itself, own, so the destination should just also refer
                // to the same (not-owned) frame
                dest.connectSocket_child_frame(src_child);
            }
        }
    }

    // delete an item from an OpenSim::Set
    template<typename T, typename TSetBase = OpenSim::Object>
    void delete_item_from_set(OpenSim::Set<T, TSetBase>& set, T* item) {
        for (int i = 0; i < set.getSize(); ++i) {
            if (&set.get(i) == item) {
                set.remove(i);
                return;
            }
        }
    }

    // draw component information as a hover tooltip
    void draw_component_hover_tooltip(OpenSim::Component const& hovered) {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() + 400.0f);

        ImGui::TextUnformatted(hovered.getName().c_str());
        ImGui::SameLine();
        ImGui::TextDisabled(" (%s)", hovered.getConcreteClassName().c_str());
        ImGui::Dummy(ImVec2{0.0f, 5.0f});
        ImGui::TextDisabled("(right-click for actions)");

        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }

    // try to delete an undoable-model's current selection
    //
    // "try", because some things are difficult to delete from OpenSim models
    void action_try_delete_selection_from_edtied_model(Undoable_ui_model& uim) {
        OpenSim::Component* selected = uim.selection();

        if (!selected) {
            return;  // nothing selected, so nothing can be deleted
        }

        if (!selected->hasOwner()) {
            // the selected item isn't owned by anything, so it can't be deleted from its
            // owner's hierarchy
            return;
        }

        OpenSim::Component* owner = const_cast<OpenSim::Component*>(&selected->getOwner());

        // else: an OpenSim::Component is selected and we need to figure out how to remove it
        // from its parent
        //
        // this is uglier than it should be because OpenSim doesn't have a uniform approach for
        // storing Components in the model hierarchy. Some Components might be in specialized sets,
        // some Components might be in std::vectors, some might be solo children, etc.
        //
        // the challenge is knowing what component is selected, what kind of parent it's contained
        // within, and how that particular component type can be safely deleted from that particular
        // parent type without leaving the overall Model in an invalid state

        if (auto* js = dynamic_cast<OpenSim::JointSet*>(owner); js) {
            // delete an OpenSim::Joint from its owning OpenSim::JointSet

            uim.before_modifying_model();
            delete_item_from_set(*js, static_cast<OpenSim::Joint*>(selected));
            uim.declare_death_of(selected);
            uim.after_modifying_model();
        } else if (auto* bs = dynamic_cast<OpenSim::BodySet*>(owner); bs) {
            // delete an OpenSim::Body from its owning OpenSim::BodySet

            log::error(
                "cannot delete %s: deleting OpenSim::Body is not supported: it segfaults in the OpenSim API",
                selected->getName().c_str());

            // segfaults:
            // uim.before_modifying_model();
            // delete_item_from_set_in_model(*bs, static_cast<OpenSim::Body*>(selected));
            // uim.model().clearConnections();
            // uim.declare_death_of(selected);
            // uim.after_modifying_model();
        } else if (auto* wos = dynamic_cast<OpenSim::WrapObjectSet*>(owner); wos) {
            // delete an OpenSim::WrapObject from its owning OpenSim::WrapObjectSet

            log::error(
                "cannot delete %s: deleting an OpenSim::WrapObject is not supported: faults in the OpenSim API until after AK's connection checking addition",
                selected->getName().c_str());

            // also, this implementation needs to iterate over all pathwraps in the model
            // and disconnect them from the GeometryPath that uses them; otherwise, the model
            // will explode
        } else if (auto* cs = dynamic_cast<OpenSim::ControllerSet*>(owner); cs) {
            // delete an OpenSim::Controller from its owning OpenSim::ControllerSet

            uim.before_modifying_model();
            delete_item_from_set(*cs, static_cast<OpenSim::Controller*>(selected));
            uim.declare_death_of(selected);
            uim.after_modifying_model();
        } else if (auto* conss = dynamic_cast<OpenSim::ConstraintSet*>(owner); cs) {
            // delete an OpenSim::Constraint from its owning OpenSim::ConstraintSet

            uim.before_modifying_model();
            delete_item_from_set(*conss, static_cast<OpenSim::Constraint*>(selected));
            uim.declare_death_of(selected);
            uim.after_modifying_model();
        } else if (auto* fs = dynamic_cast<OpenSim::ForceSet*>(owner); fs) {
            // delete an OpenSim::Force from its owning OpenSim::ForceSet

            uim.before_modifying_model();
            delete_item_from_set(*fs, static_cast<OpenSim::Force*>(selected));
            uim.declare_death_of(selected);
            uim.after_modifying_model();
        } else if (auto* ms = dynamic_cast<OpenSim::MarkerSet*>(owner); ms) {
            // delete an OpenSim::Marker from its owning OpenSim::MarkerSet

            uim.before_modifying_model();
            delete_item_from_set(*ms, static_cast<OpenSim::Marker*>(selected));
            uim.declare_death_of(selected);
            uim.after_modifying_model();
        } else if (auto* cgs = dynamic_cast<OpenSim::ContactGeometrySet*>(owner); cgs) {
            // delete an OpenSim::ContactGeometry from its owning OpenSim::ContactGeometrySet

            uim.before_modifying_model();
            delete_item_from_set(*cgs, static_cast<OpenSim::ContactGeometry*>(selected));
            uim.declare_death_of(selected);
            uim.after_modifying_model();
        } else if (auto* ps = dynamic_cast<OpenSim::ProbeSet*>(owner); ps) {
            // delete an OpenSim::Probe from its owning OpenSim::ProbeSet

            uim.before_modifying_model();
            delete_item_from_set(*ps, static_cast<OpenSim::Probe*>(selected));
            uim.declare_death_of(selected);
            uim.after_modifying_model();
        } else if (auto const* geom = find_ancestor<OpenSim::Geometry>(selected); geom) {
            // delete an OpenSim::Geometry from its owning OpenSim::Frame

            if (auto const* frame = find_ancestor<OpenSim::Frame>(geom); frame) {
                // its owner is a frame, which holds the geometry in a list property

                // make a copy of the property containing the geometry and
                // only copy over the not-deleted geometry into the copy
                //
                // this is necessary because OpenSim::Property doesn't seem
                // to support list element deletion, but does support full
                // assignment
                auto& mframe = const_cast<OpenSim::Frame&>(*frame);
                OpenSim::ObjectProperty<OpenSim::Geometry>& prop =
                    static_cast<OpenSim::ObjectProperty<OpenSim::Geometry>&>(mframe.updProperty_attached_geometry());

                std::unique_ptr<OpenSim::ObjectProperty<OpenSim::Geometry>> copy{prop.clone()};
                copy->clear();
                for (int i = 0; i < prop.size(); ++i) {
                    OpenSim::Geometry& g = prop[i];
                    if (&g != geom) {
                        copy->adoptAndAppendValue(g.clone());
                    }
                }

                uim.before_modifying_model();
                prop.assign(*copy);
                uim.declare_death_of(selected);
                uim.after_modifying_model();
            }
        } else if (auto* pp = dynamic_cast<OpenSim::PathPoint*>(selected); pp) {
            if (auto* gp = dynamic_cast<OpenSim::GeometryPath*>(owner); gp) {

                OpenSim::PathPointSet const& pps = gp->getPathPointSet();
                int idx = -1;
                for (int i = 0; i < pps.getSize(); ++i) {
                    if (&pps.get(i) == pp) {
                        idx = i;
                    }
                }

                if (idx != -1) {
                    uim.before_modifying_model();
                    gp->deletePathPoint(uim.state(), idx);
                    uim.declare_death_of(selected);
                    uim.after_modifying_model();
                }
            }
        }
    }

    // draw an editor for top-level selected Component members (e.g. name)
    void draw_top_level_members_editor(Undoable_ui_model& st) {
        if (!st.selection()) {
            ImGui::Text("cannot draw top level editor: nothing selected?");
            return;
        }
        OpenSim::Component& selection = *st.selection();

        ImGui::Columns(2);

        ImGui::TextUnformatted("name");
        ImGui::NextColumn();

        char nambuf[128];
        nambuf[sizeof(nambuf) - 1] = '\0';
        std::strncpy(nambuf, selection.getName().c_str(), sizeof(nambuf) - 1);
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
        if (ImGui::InputText("##nameditor", nambuf, sizeof(nambuf), ImGuiInputTextFlags_EnterReturnsTrue)) {

            if (std::strlen(nambuf) > 0) {
                st.before_modifying_model();
                selection.setName(nambuf);
                st.after_modifying_model();
            }
        }
        ImGui::NextColumn();

        ImGui::Columns();
    }

    // draw UI element that lets user change a model joint's type
    void draw_joint_type_switcher(Undoable_ui_model& st, OpenSim::Joint& selection) {
        auto const* parent_jointset =
            selection.hasOwner() ? dynamic_cast<OpenSim::JointSet const*>(&selection.getOwner()) : nullptr;

        if (!parent_jointset) {
            // it's a joint, but it's not owned by a JointSet, so the implementation cannot switch
            // the joint type
            return;
        }

        OpenSim::JointSet const& js = *parent_jointset;

        int idx = -1;
        for (int i = 0; i < js.getSize(); ++i) {
            OpenSim::Joint const* j = &js[i];
            if (j == &selection) {
                idx = i;
                break;
            }
        }

        if (idx == -1) {
            // logically, this should never happen
            return;
        }

        ImGui::TextUnformatted("joint type");
        ImGui::NextColumn();

        // look the Joint up in the type registry so we know where it should be in the ImGui::Combo
        std::optional<size_t> maybe_type_idx = Joint_registry::index_of(selection);
        int type_idx = maybe_type_idx ? static_cast<int>(*maybe_type_idx) : -1;

        auto known_joint_names = Joint_registry::names();

        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
        if (ImGui::Combo(
                "##newjointtypeselector",
                &type_idx,
                known_joint_names.data(),
                static_cast<int>(known_joint_names.size())) &&
            type_idx >= 0) {

            // copy + fixup  a prototype of the user's selection
            std::unique_ptr<OpenSim::Joint> new_joint{Joint_registry::prototypes()[static_cast<size_t>(type_idx)]->clone()};
            copy_common_joint_properties(selection, *new_joint);

            // overwrite old joint in model
            //
            // note: this will invalidate the `selection` joint, because the
            // OpenSim::JointSet container will automatically kill it
            st.before_modifying_model();
            OpenSim::Joint* ptr = new_joint.get();
            const_cast<OpenSim::JointSet&>(js).set(idx, new_joint.release());
            st.declare_death_of(&selection);
            st.set_selection(ptr);
            st.after_modifying_model();
        }
        ImGui::NextColumn();
    }


    // try to undo currently edited model to earlier state
    void action_undo_currently_edited_model(Main_editor_state& mes) {
        if (mes.edited_model.can_undo()) {
            mes.edited_model.do_undo();
        }
    }

    // try to redo currently edited model to later state
    void action_redo_currently_edited_model(Main_editor_state& mes) {
        if (mes.edited_model.can_redo()) {
            mes.edited_model.do_redo();
        }
    }

    // disable all wrapping surfaces in the current model
    void action_disable_all_wrapping_surfs(Main_editor_state& mes) {
        Undoable_ui_model& uim = mes.edited_model;

        OpenSim::Model& m = uim.model();

        uim.before_modifying_model();
        for (OpenSim::WrapObjectSet& wos : m.updComponentList<OpenSim::WrapObjectSet>()) {
            for (int i = 0; i < wos.getSize(); ++i) {
                OpenSim::WrapObject& wo = wos[i];
                wo.set_active(false);
                wo.upd_Appearance().set_visible(false);
            }
        }
        uim.after_modifying_model();
    }

    // enable all wrapping surfaces in the current model
    void action_enable_all_wrapping_surfs(Main_editor_state& mes) {
        Undoable_ui_model& uim = mes.edited_model;

        OpenSim::Model& m = uim.model();

        uim.before_modifying_model();
        for (OpenSim::WrapObjectSet& wos : m.updComponentList<OpenSim::WrapObjectSet>()) {
            for (int i = 0; i < wos.getSize(); ++i) {
                OpenSim::WrapObject& wo = wos[i];
                wo.set_active(true);
                wo.upd_Appearance().set_visible(true);
            }
        }
        uim.after_modifying_model();
    }

    // try to start a new simulation from the currently-edited model
    void action_start_sim_from_edited_model(Main_editor_state& mes) {
        mes.start_simulating_edited_model();
    }

    void action_clear_selection_from_edited_model(Main_editor_state& mes) {
        mes.edited_model.set_selection(nullptr);
    }
}

// editor (internal) screen state
struct Model_editor_screen::Impl final {

    // top-level state this screen can handle
    std::shared_ptr<Main_editor_state> st;

    // polls changes to a file
    File_change_poller file_poller;

    // internal state of any sub-panels the editor screen draws
    struct {
        ui::main_menu::file_tab::State main_menu_tab;
        ui::add_body_popup::State abm;
        ui::properties_editor::State properties_editor;
        ui::reassign_socket::State reassign_socket;
        ui::attach_geometry_popup::State attach_geometry_modal;
        ui::select_2_pfs::State select_2_pfs;
        ui::model_actions::State model_actions_panel;
        ui::log_viewer::State log_viewer;
    } ui;

    // state that is reset at the start of each frame
    struct {
        bool edit_sim_params_requested = false;
        bool subpanel_requested_early_exit = false;
    } reset_per_frame;

    explicit Impl(std::shared_ptr<Main_editor_state> _st) :
        st{std::move(_st)},
        file_poller{std::chrono::milliseconds{1000}, st->edited_model.model().getInputFileName()} {
    }
};

namespace {

    // handle what happens when a user presses a key
    bool modeleditor_on_keydown(Model_editor_screen::Impl& impl, SDL_KeyboardEvent const& e) {

        if (e.keysym.mod & KMOD_CTRL) {  // Ctrl
            if (e.keysym.mod & KMOD_SHIFT) {  // Ctrl+Shift
                switch (e.keysym.sym) {
                case SDLK_z:  // Ctrl+Shift+Z : undo focused model
                    action_redo_currently_edited_model(*impl.st);
                    return true;
                }
                return false;
            }

            switch (e.keysym.sym) {
            case SDLK_z:  // Ctrl+Z: undo focused model
                action_undo_currently_edited_model(*impl.st);
                return true;
            case SDLK_r:  // Ctrl+R: start a new simulation from focused model
                action_start_sim_from_edited_model(*impl.st);
                return true;
            case SDLK_a:  // Ctrl+A: clear selection
                action_clear_selection_from_edited_model(*impl.st);
                return true;
            case SDLK_e:  // Ctrl+E: show simulation screen
                App::cur().request_transition<Simulator_screen>(std::move(impl.st));
                return true;
            }

            return false;
        }

        switch (e.keysym.sym) {
        case SDLK_DELETE:  // DELETE: delete selection
            action_try_delete_selection_from_edtied_model(impl.st->edited_model);
            return true;
        }

        return false;
    }

    // handle what happens when the underlying model file changes
    void modeleditor_on_backing_file_changed(Model_editor_screen::Impl& impl) {
        try {
            log::info("file change detected: loading updated file");
            auto p = std::make_unique<OpenSim::Model>(impl.st->model().getInputFileName());
            log::info("loaded updated file");
            impl.st->set_model(std::move(p));
        } catch (std::exception const& ex) {
            log::error("error occurred while trying to automatically load a model file:");
            log::error(ex.what());
            log::error("the file will not be loaded into osc (you won't see the change in the UI)");
        }
    }

    // handle what happens when a generic event arrives in the screen
    bool modeleditor_on_event(Model_editor_screen::Impl& impl, SDL_Event const& e) {
        bool handled = false;

        if (e.type == SDL_KEYDOWN) {
            handled = modeleditor_on_keydown(impl, e.key);
        }

        return handled;
    }

    // tick the screen forward
    void modeleditor_tick(Model_editor_screen::Impl& impl) {
        if (impl.file_poller.change_detected(impl.st->model().getInputFileName())) {
            modeleditor_on_backing_file_changed(impl);
        }
    }

    // draw contextual actions (buttons, sliders) for a selected physical frame
    void draw_physicalframe_contextual_actions(
            Model_editor_screen::Impl& impl,
            Undoable_ui_model& uim,
            OpenSim::PhysicalFrame& selection) {

        ImGui::Columns(2);

        ImGui::TextUnformatted("geometry");
        ImGui::SameLine();
        ui::help_marker::draw("Geometry that is attached to this physical frame. Multiple pieces of geometry can be attached to the frame");
        ImGui::NextColumn();

        static constexpr char const* modal_name = "select geometry to add";

        if (ImGui::Button("add geometry")) {
            ImGui::OpenPopup(modal_name);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted("Add geometry to this component. Geometry can be removed by selecting it in the hierarchy editor and pressing DELETE");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        if (auto attached = ui::attach_geometry_popup::draw(impl.ui.attach_geometry_modal, modal_name); attached) {
            uim.before_modifying_model();
            selection.attachGeometry(attached.release());
            uim.after_modifying_model();
        }
        ImGui::NextColumn();

        ImGui::TextUnformatted("offset frame");
        ImGui::NextColumn();
        if (ImGui::Button("add offset frame")) {
            auto pof = std::make_unique<OpenSim::PhysicalOffsetFrame>();
            pof->setName(selection.getName() + "_offsetframe");
            pof->setParentFrame(selection);

            uim.before_modifying_model();
            auto pofptr = pof.get();
            selection.addComponent(pof.release());
            uim.set_selection(pofptr);
            uim.after_modifying_model();
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted("Add an OpenSim::OffsetFrame as a child of this Component. Other components in the model can then connect to this OffsetFrame, rather than the base Component, so that it can connect at some offset that is relative to the parent Component");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::NextColumn();

        ImGui::Columns();
    }

    // draw contextual actions (buttons, sliders) for a selected joint
    void draw_joint_contextual_actions(
            Undoable_ui_model& st,
            OpenSim::Joint& selection) {

        ImGui::Columns(2);

        draw_joint_type_switcher(st, selection);

        // BEWARE: broke
        {
            ImGui::Text("add offset frame");
            ImGui::NextColumn();

            if (ImGui::Button("parent")) {
                auto pf = std::make_unique<OpenSim::PhysicalOffsetFrame>();
                pf->setParentFrame(selection.getParentFrame());

                st.before_modifying_model();
                selection.addFrame(pf.release());
                st.after_modifying_model();
            }
            ImGui::SameLine();
            if (ImGui::Button("child")) {
                auto pf = std::make_unique<OpenSim::PhysicalOffsetFrame>();
                pf->setParentFrame(selection.getChildFrame());

                st.before_modifying_model();
                selection.addFrame(pf.release());
                st.after_modifying_model();
            }
            ImGui::NextColumn();
        }

        ImGui::Columns();
    }

    // draw contextual actions (buttons, sliders) for a selected joint
    void draw_hcf_contextual_actions(
        Undoable_ui_model& uim,
        OpenSim::HuntCrossleyForce& selection) {

        if (selection.get_contact_parameters().getSize() > 1) {
            ImGui::Text("cannot edit: has more than one HuntCrossleyForce::Parameter");
            return;
        }

        // HACK: if it has no parameters, give it some. The HuntCrossleyForce implementation effectively
        // does this internally anyway to satisfy its own API (e.g. `getStaticFriction` requires that
        // the HuntCrossleyForce has a parameter)
        if (selection.get_contact_parameters().getSize() == 0) {
            selection.updContactParametersSet().adoptAndAppend(new OpenSim::HuntCrossleyForce::ContactParameters());
        }

        OpenSim::HuntCrossleyForce::ContactParameters& params = selection.upd_contact_parameters()[0];

        ImGui::Columns(2);
        ImGui::TextUnformatted("add contact geometry");
        ImGui::SameLine();
        ui::help_marker::draw(
            "Add OpenSim::ContactGeometry to this OpenSim::HuntCrossleyForce.\n\nCollisions are evaluated for all OpenSim::ContactGeometry attached to the OpenSim::HuntCrossleyForce. E.g. if you want an OpenSim::ContactSphere component to collide with an OpenSim::ContactHalfSpace component during a simulation then you should add both of those components to this force");
        ImGui::NextColumn();

        // allow user to add geom
        {
            if (ImGui::Button("add contact geometry")) {
                ImGui::OpenPopup("select contact geometry");
            }

            OpenSim::ContactGeometry const* added =
                ui::select_component::draw<OpenSim::ContactGeometry>("select contact geometry", uim.model());

            if (added) {
                uim.before_modifying_model();
                params.updGeometry().appendValue(added->getName());
                uim.after_modifying_model();
            }
        }

        ImGui::NextColumn();
        ImGui::Columns();

        // render standard, easy to render, props of the contact params
        {
            std::array<int, 6> easy_to_handle_props = {
                params.PropertyIndex_geometry,
                params.PropertyIndex_stiffness,
                params.PropertyIndex_dissipation,
                params.PropertyIndex_static_friction,
                params.PropertyIndex_dynamic_friction,
                params.PropertyIndex_viscous_friction,
            };

            ui::properties_editor::State st;
            auto maybe_updater = ui::properties_editor::draw(st, params, easy_to_handle_props);

            if (maybe_updater) {
                uim.before_modifying_model();
                maybe_updater->updater(const_cast<OpenSim::AbstractProperty&>(maybe_updater->prop));
                uim.after_modifying_model();
            }
        }
    }

    // draw contextual actions (buttons, sliders) for a selected path actuator
    void draw_pa_contextual_actions(
            Undoable_ui_model& uim,
            OpenSim::PathActuator& selection) {

        ImGui::Columns(2);

        char const* modal_name = "select physical frame";

        ImGui::TextUnformatted("add path point to end");
        ImGui::NextColumn();

        if (ImGui::Button("add")) {
            ImGui::OpenPopup(modal_name);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(
                "Add a new path point, attached to an OpenSim::PhysicalFrame in the model, to the end of the sequence of path points in this OpenSim::PathActuator");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        // handle popup
        {
            OpenSim::PhysicalFrame const* pf = ui::select_1_pf::draw(modal_name, uim.model());
            if (pf) {
                int n = selection.getGeometryPath().getPathPointSet().getSize();
                char buf[128];
                std::snprintf(buf, sizeof(buf), "%s-P%i", selection.getName().c_str(), n + 1);
                std::string name{buf};
                SimTK::Vec3 pos{0.0f, 0.0f, 0.0f};

                uim.before_modifying_model();
                selection.addNewPathPoint(name, *pf, pos);
                uim.after_modifying_model();
            }
        }

        ImGui::NextColumn();
        ImGui::Columns();
    }

    // draw contextual actions for selection
    void draw_contextual_actions(Model_editor_screen::Impl& impl, Undoable_ui_model& uim) {

        if (!uim.selection()) {
            ImGui::TextUnformatted("cannot draw contextual actions: selection is blank (shouldn't be)");
            return;
        }

        ImGui::Columns(2);
        ImGui::TextUnformatted("isolate in visualizer");
        ImGui::NextColumn();

        if (uim.selection() != uim.isolated()) {
            if (ImGui::Button("isolate")) {
                uim.before_modifying_model();
                uim.set_isolated(uim.selection());
                uim.after_modifying_model();
            }
        } else {
            if (ImGui::Button("clear isolation")) {
                uim.before_modifying_model();
                uim.set_isolated(nullptr);
                uim.after_modifying_model();
            }
        }

        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(
                "Only show this component in the visualizer\n\nThis can be disabled from the Edit menu (Edit -> Clear Isolation)");
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
        ImGui::NextColumn();
        ImGui::Columns();

        if (auto* frame = dynamic_cast<OpenSim::PhysicalFrame*>(uim.selection()); frame) {
            draw_physicalframe_contextual_actions(impl, uim, *frame);
        } else if (auto* joint = dynamic_cast<OpenSim::Joint*>(uim.selection()); joint) {
            draw_joint_contextual_actions(uim, *joint);
        } else if (auto* hcf = dynamic_cast<OpenSim::HuntCrossleyForce*>(uim.selection()); hcf) {
            draw_hcf_contextual_actions(uim, *hcf);
        } else if (auto* pa = dynamic_cast<OpenSim::PathActuator*>(uim.selection()); pa) {
            draw_pa_contextual_actions(uim, *pa);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Text, OSC_GREYED_RGBA);
            ImGui::Text(
                "    (OpenSim::%s has no contextual actions)", uim.selection()->getConcreteClassName().c_str());
            ImGui::PopStyleColor();
        }
    }


    // draw socket editor for current selection
    void draw_socket_editor(Model_editor_screen::Impl& impl, Undoable_ui_model& uim) {

        if (uim.selection()) {
            ImGui::TextUnformatted("cannot draw socket editor: selection is blank (shouldn't be)");
            return;
        }

        std::vector<std::string> socknames = uim.selection()->getSocketNames();

        if (socknames.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, OSC_GREYED_RGBA);
            ImGui::Text("    (OpenSim::%s has no sockets)", uim.selection()->getConcreteClassName().c_str());
            ImGui::PopStyleColor();
            return;
        }

        // else: it has sockets with names, list each socket and provide the user
        //       with the ability to reassign the socket's connectee

        ImGui::Columns(2);
        for (std::string const& sn : socknames) {
            ImGui::TextUnformatted(sn.c_str());
            ImGui::NextColumn();

            OpenSim::AbstractSocket const& socket = uim.selection()->getSocket(sn);
            std::string sockname = socket.getConnecteePath();
            std::string popupname = std::string{"reassign"} + sockname;

            if (ImGui::Button(sockname.c_str())) {
                ImGui::OpenPopup(popupname.c_str());
            }

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::Text(
                    "%s\n\nClick to reassign this socket's connectee",
                    socket.getConnecteeAsObject().getConcreteClassName().c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }

            if (OpenSim::Object const* connectee =
                    ui::reassign_socket::draw(impl.ui.reassign_socket, popupname.c_str(), uim.model(), socket);
                connectee) {

                ImGui::CloseCurrentPopup();

                OpenSim::Object const& existing = socket.getConnecteeAsObject();
                uim.before_modifying_model();
                try {
                    uim.selection()->updSocket(sn).connect(*connectee);
                    impl.ui.reassign_socket.search[0] = '\0';
                    impl.ui.reassign_socket.error.clear();
                    ImGui::CloseCurrentPopup();
                } catch (std::exception const& ex) {
                    impl.ui.reassign_socket.error = ex.what();
                    uim.selection()->updSocket(sn).connect(existing);
                }
                uim.after_modifying_model();
            }

            ImGui::NextColumn();
        }
        ImGui::Columns();
    }

    // draw breadcrumbs for current selection
    //
    // eg: Model > Joint > PhysicalFrame
    void draw_selection_breadcrumbs(Undoable_ui_model& uim) {
        if (!uim.selection()) {
            return;  // nothing selected
        }

        auto lst = osc::path_to(*uim.selection());

        if (lst.empty()) {
            return;  // this shouldn't happen, but you never know...
        }

        float indent = 0.0f;

        for (auto it = lst.begin(); it != lst.end() - 1; ++it) {
            ImGui::Dummy(ImVec2{indent, 0.0f});
            ImGui::SameLine();
            if (ImGui::Button((*it)->getName().c_str())) {
                uim.set_selection(const_cast<OpenSim::Component*>(*it));
            }
            if (ImGui::IsItemHovered()) {
                uim.set_hovered(const_cast<OpenSim::Component*>(*it));
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::Text("OpenSim::%s", (*it)->getConcreteClassName().c_str());
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }
            ImGui::SameLine();
            ImGui::TextDisabled("(%s)", (*it)->getConcreteClassName().c_str());
            indent += 15.0f;
        }

        ImGui::Dummy(ImVec2{indent, 0.0f});
        ImGui::SameLine();
        ImGui::TextUnformatted((*(lst.end() - 1))->getName().c_str());
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", (*(lst.end() - 1))->getConcreteClassName().c_str());
    }

    // draw editor for current selection
    void draw_selection_editor(Model_editor_screen::Impl& impl, Undoable_ui_model& uim) {
        if (!uim.selection()) {
            ImGui::TextUnformatted("(nothing selected)");
            return;
        }

        ImGui::Dummy(ImVec2(0.0f, 1.0f));
        ImGui::TextUnformatted("hierarchy:");
        ImGui::SameLine();
        ui::help_marker::draw("Where the selected component is in the model's component hierarchy");
        ImGui::Separator();
        draw_selection_breadcrumbs(uim);

        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::TextUnformatted("top-level attributes:");
        ImGui::SameLine();
        ui::help_marker::draw("Top-level properties on the OpenSim::Component itself");
        ImGui::Separator();
        draw_top_level_members_editor(uim);

        // contextual actions
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::TextUnformatted("contextual actions:");
        ImGui::SameLine();
        ui::help_marker::draw("Actions that are specific to the type of OpenSim::Component that is currently selected");
        ImGui::Separator();
        draw_contextual_actions(impl, uim);

        // a contextual action may have changed this
        if (!uim.selection()) {
            return;
        }

        // property editor
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::TextUnformatted("properties:");
        ImGui::SameLine();
        ui::help_marker::draw(
            "Properties of the selected OpenSim::Component. These are declared in the Component's implementation.");
        ImGui::Separator();
        {
            auto maybe_updater = ui::properties_editor::draw(impl.ui.properties_editor, *uim.selection());
            if (maybe_updater) {
                uim.before_modifying_model();
                maybe_updater->updater(const_cast<OpenSim::AbstractProperty&>(maybe_updater->prop));
                uim.after_modifying_model();
            }
        }

        // socket editor
        ImGui::Dummy(ImVec2(0.0f, 2.0f));
        ImGui::TextUnformatted("sockets:");
        ImGui::SameLine();
        ui::help_marker::draw(
            "What components this component is connected to.\n\nIn OpenSim, a Socket formalizes the dependency between a Component and another object (typically another Component) without owning that object. While Components can be composites (of multiple components) they often depend on unrelated objects/components that are defined and owned elsewhere. The object that satisfies the requirements of the Socket we term the 'connectee'. When a Socket is satisfied by a connectee we have a successful 'connection' or is said to be connected.");
        ImGui::Separator();
        draw_socket_editor(impl, uim);
    }

    // draw the "Actions" tab of the main (top) menu
    void draw_main_menu_edit_tab(Model_editor_screen::Impl& impl) {

        Undoable_ui_model& uim = impl.st->edited_model;

        if (ImGui::BeginMenu("Edit")) {

            if (ImGui::MenuItem(ICON_FA_UNDO " Undo", "Ctrl+Z", false, uim.can_undo())) {
                action_undo_currently_edited_model(*impl.st);
            }

            if (ImGui::MenuItem(ICON_FA_REDO " Redo", "Ctrl+Shift+Z", false, uim.can_redo())) {
                action_redo_currently_edited_model(*impl.st);
            }

            if (ImGui::MenuItem(ICON_FA_EYE_SLASH " Clear Isolation", nullptr, false, uim.isolated())) {
                uim.set_isolated(nullptr);
            }

            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted("Clear currently isolation setting. This is effectively the opposite of 'Isolate'ing a component.");
                if (!uim.isolated()) {
                    ImGui::TextDisabled("\n(disabled because nothing is currently isolated)");
                }
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }

            if (ImGui::MenuItem(ICON_FA_LINK " Open in external editor", nullptr, false, has_backing_file(impl.st->edited_model.model()))) {
                open_path_in_default_application(uim.model().getInputFileName());
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
                ImGui::TextUnformatted("Open the .osim file currently being edited in an external text editor. The editor that's used depends on your operating system's default for opening .osim files.");
                if (!has_backing_file(uim.model())) {
                    ImGui::TextDisabled("\n(disabled because the currently-edited model has no backing file)");
                }
                ImGui::PopTextWrapPos();
                ImGui::EndTooltip();
            }

            ImGui::EndMenu();
        }
    }

    void draw_main_menu_simulate_tab(Model_editor_screen::Impl& impl) {
        if (ImGui::BeginMenu("Tools")) {
            if (ImGui::MenuItem(ICON_FA_PLAY " Simulate", "Ctrl+R")) {
                impl.st->start_simulating_edited_model();
                App::cur().request_transition<Simulator_screen>(impl.st);
                impl.reset_per_frame.subpanel_requested_early_exit = true;
            }

            if (ImGui::MenuItem(ICON_FA_EDIT " Edit simulation settings")) {
                impl.reset_per_frame.edit_sim_params_requested = true;
            }

            if (ImGui::MenuItem("Disable all wrapping surfaces")) {
                action_disable_all_wrapping_surfs(*impl.st);
            }

            if (ImGui::MenuItem("Enable all wrapping surfaces")) {
                action_enable_all_wrapping_surfs(*impl.st);
            }

            ImGui::EndMenu();
        }
    }

    // draws the screen's main menu
    void draw_main_menu(Model_editor_screen::Impl& impl) {
        if (ImGui::BeginMainMenuBar()) {
            ui::main_menu::file_tab::draw(impl.ui.main_menu_tab, impl.st);
            draw_main_menu_edit_tab(impl);
            draw_main_menu_simulate_tab(impl);
            ui::main_menu::window_tab::draw(*impl.st);
            ui::main_menu::about_tab::draw();


            ImGui::Dummy(ImVec2{2.0f, 0.0f});
            if (ImGui::Button(ICON_FA_LIST_ALT " Switch to simulator (Ctrl+E)")) {
                App::cur().request_transition<Simulator_screen>(std::move(impl.st));
                ImGui::EndMainMenuBar();
                impl.reset_per_frame.subpanel_requested_early_exit = true;
                return;
            }

            // "switch to simulator" menu button
            ImGui::PushStyleColor(ImGuiCol_Button, OSC_POSITIVE_RGBA);
            if (ImGui::Button(ICON_FA_PLAY " Simulate (Ctrl+R)")) {
                impl.st->start_simulating_edited_model();
                App::cur().request_transition<Simulator_screen>(std::move(impl.st));
                ImGui::PopStyleColor();
                ImGui::EndMainMenuBar();
                impl.reset_per_frame.subpanel_requested_early_exit = true;
                return;
            }
            ImGui::PopStyleColor();

            if (ImGui::Button(ICON_FA_EDIT " Edit simulation settings")) {
                impl.reset_per_frame.edit_sim_params_requested = true;
            }

            ImGui::EndMainMenuBar();
        }
    }

    // draw right-click context menu for the 3D viewer
    void draw_3dviewer_context_menu(
            Model_editor_screen::Impl& impl,
            OpenSim::Component const& selected) {

        ImGui::TextDisabled("%s (%s)", selected.getName().c_str(), selected.getConcreteClassName().c_str());
        ImGui::Separator();
        ImGui::Dummy(ImVec2{0.0f, 3.0f});

        if (ImGui::BeginMenu("Select Owner")) {
            OpenSim::Component const* c = &selected;
            impl.st->set_hovered(nullptr);
            while (c->hasOwner()) {
                c = &c->getOwner();

                char buf[128];
                std::snprintf(buf, sizeof(buf), "%s (%s)", c->getName().c_str(), c->getConcreteClassName().c_str());

                if (ImGui::MenuItem(buf)) {
                    impl.st->set_selection(const_cast<OpenSim::Component*>(c));
                }
                if (ImGui::IsItemHovered()) {
                    impl.st->set_hovered(const_cast<OpenSim::Component*>(c));
                }
            }
            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Request Outputs")) {
            ui::help_marker::draw("Request that these outputs are plotted whenever a simulation is ran. The outputs will appear in the 'outputs' tab on the simulator screen");

            OpenSim::Component const* c = &selected;
            int imgui_id = 0;
            while (c) {
                ImGui::PushID(imgui_id++);
                ImGui::Dummy(ImVec2{0.0f, 2.0f});
                ImGui::TextDisabled("%s (%s)", c->getName().c_str(), c->getConcreteClassName().c_str());
                ImGui::Separator();
                for (auto const& o : c->getOutputs()) {
                    char buf[256];
                    std::snprintf(buf, sizeof(buf), "  %s", o.second->getName().c_str());

                    auto const& suboutputs = get_subfields(*o.second);
                    if (suboutputs.empty()) {
                        // can only plot top-level of output

                        if (ImGui::MenuItem(buf)) {
                           impl.st->desired_outputs.emplace_back(*c, *o.second);
                        }
                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            ImGui::Text("Output Type = %s", o.second->getTypeName().c_str());
                            ImGui::EndTooltip();
                        }
                    } else {
                        // can plot suboutputs
                        if (ImGui::BeginMenu(buf)) {
                            for (Plottable_output_subfield const& pos : suboutputs) {
                                if (ImGui::MenuItem(pos.name)) {
                                    impl.st->desired_outputs.emplace_back(*c, *o.second, pos);
                                }
                            }
                            ImGui::EndMenu();
                        }

                        if (ImGui::IsItemHovered()) {
                            ImGui::BeginTooltip();
                            ImGui::Text("Output Type = %s", o.second->getTypeName().c_str());
                            ImGui::EndTooltip();
                        }
                    }
                }
                if (c->getNumOutputs() == 0) {
                    ImGui::TextDisabled("  (has no outputs)");
                }
                ImGui::PopID();
                c = c->hasOwner() ? &c->getOwner() : nullptr;
            }
            ImGui::EndMenu();
        }
    }

    // draw a single 3D model viewer
    void draw_3dviewer(
            Model_editor_screen::Impl& impl,
            Component_3d_viewer& viewer,
            char const* name) {

        Component3DViewerResponse resp;

        if (impl.st->isolated()) {
            resp = viewer.draw(
                name,
                *impl.st->isolated(),
                impl.st->model().getDisplayHints(),
                impl.st->state(),
                impl.st->selection(),
                impl.st->hovered());
        } else {
            resp = viewer.draw(
                name,
                impl.st->model(),
                impl.st->state(),
                impl.st->selection(),
                impl.st->hovered());
        }

        // update hover
        if (resp.is_moused_over && resp.hovertest_result != impl.st->hovered()) {
            impl.st->set_hovered(const_cast<OpenSim::Component*>(resp.hovertest_result));
        }

        // if left-clicked, update selection
        if (resp.is_moused_over && resp.is_left_clicked) {
            impl.st->set_selection(const_cast<OpenSim::Component*>(resp.hovertest_result));
        }

        // if hovered, draw hover tooltip
        if (resp.is_moused_over && resp.hovertest_result) {
            draw_component_hover_tooltip(*resp.hovertest_result);
        }

        // if right-clicked, draw context menu
        {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "%s_contextmenu", name);
            if (resp.is_moused_over && resp.hovertest_result && resp.is_right_clicked) {
                impl.st->set_selection(const_cast<OpenSim::Component*>(resp.hovertest_result));
                ImGui::OpenPopup(buf);
            }
            if (impl.st->selection() && ImGui::BeginPopup(buf)) {
                draw_3dviewer_context_menu(impl, *impl.st->selection());
                ImGui::EndPopup();
            }
        }
    }

    // draw all user-enabled 3D model viewers
    void draw_3dviewers(Model_editor_screen::Impl& impl) {
        for (size_t i = 0; i < impl.st->viewers.size(); ++i) {
            auto& maybe_viewer = impl.st->viewers[i];

            if (!maybe_viewer) {
                continue;
            }

            Component_3d_viewer& viewer = *maybe_viewer;

            char buf[64];
            std::snprintf(buf, sizeof(buf), "viewer%zu", i);

            draw_3dviewer(impl, viewer, buf);
        }
    }

    // draw model editor screen
    //
    // can throw if the model is in an invalid state
    void modeleditor_draw_UNGUARDED(Model_editor_screen::Impl& impl) {

        impl.reset_per_frame = {};

        // draw main menu
        {
            draw_main_menu(impl);
        }

        // check for early exit request
        //
        // (the main menu may have requested a screen transition)
        if (impl.reset_per_frame.subpanel_requested_early_exit) {
            return;
        }

        // draw 3D viewers (if any)
        {
            draw_3dviewers(impl);
        }

        // draw editor actions panel
        //
        // contains top-level actions (e.g. "add body")
        if (impl.st->showing.actions) {
            if (ImGui::Begin("Actions", nullptr, ImGuiWindowFlags_MenuBar)) {
                auto on_set_selection = [&](OpenSim::Component* c) { impl.st->edited_model.set_selection(c); };
                auto before_modify_model = [&]() { impl.st->edited_model.before_modifying_model(); };
                auto after_modify_model = [&]() { impl.st->edited_model.after_modifying_model(); };
                ui::model_actions::draw(
                    impl.ui.model_actions_panel, impl.st->edited_model.model(), on_set_selection, before_modify_model, after_modify_model);
            }
            ImGui::End();
        }

        // draw hierarchy viewer
        if (impl.st->showing.hierarchy) {
            if (ImGui::Begin("Hierarchy", &impl.st->showing.hierarchy)) {
                auto resp = ui::component_hierarchy::draw(
                    &impl.st->edited_model.model().getRoot(),
                    impl.st->edited_model.selection(),
                    impl.st->edited_model.hovered());

                if (resp.type == ui::component_hierarchy::SelectionChanged) {
                    impl.st->set_selection(const_cast<OpenSim::Component*>(resp.ptr));
                } else if (resp.type == ui::component_hierarchy::HoverChanged) {
                    impl.st->set_hovered(const_cast<OpenSim::Component*>(resp.ptr));
                }
            }
            ImGui::End();
        }

        // draw selection details
        if (impl.st->showing.selection_details) {
            if (ImGui::Begin("Selection", &impl.st->showing.selection_details)) {
                auto resp = ui::component_details::draw(impl.st->edited_model.state(), impl.st->edited_model.selection());

                if (resp.type == ui::component_details::SelectionChanged) {
                    impl.st->edited_model.set_selection(const_cast<OpenSim::Component*>(resp.ptr));
                }
            }
            ImGui::End();
        }

        // draw property editor
        if (impl.st->showing.property_editor) {
            if (ImGui::Begin("Edit Props", &impl.st->showing.property_editor)) {
                draw_selection_editor(impl, impl.st->edited_model);
            }
            ImGui::End();
        }

        // draw application log
        if (impl.st->showing.log) {
            if (ImGui::Begin("Log", &impl.st->showing.log, ImGuiWindowFlags_MenuBar)) {
                ui::log_viewer::draw(impl.ui.log_viewer);
            }
            ImGui::End();
        }

        // draw sim params editor popup (if applicable)
        {
            if (impl.reset_per_frame.edit_sim_params_requested) {
                ImGui::OpenPopup("simulation parameters");
            }

            osc::ui::fd_params_editor_popup::draw("simulation parameters", impl.st->sim_params);
        }

        if (impl.reset_per_frame.subpanel_requested_early_exit) {
            return;
        }

        // garbage-collect any models damaged by in-UI modifications (if applicable)
        impl.st->clear_any_damaged_models();
    }
}


// public API

osc::Model_editor_screen::Model_editor_screen(std::shared_ptr<Main_editor_state> st) :
    impl{new Impl {std::move(st)}} {
}

osc::Model_editor_screen::Model_editor_screen(Model_editor_screen&&) noexcept = default;
osc::Model_editor_screen& osc::Model_editor_screen::operator=(Model_editor_screen&&) noexcept = default;
osc::Model_editor_screen::~Model_editor_screen() noexcept = default;

void osc::Model_editor_screen::on_mount() {
    osc::ImGuiInit();
}

void osc::Model_editor_screen::on_unmount() {
    osc::ImGuiShutdown();
}

void Model_editor_screen::on_event(SDL_Event const& e) {
    if (osc::ImGuiOnEvent(e)) {
        return;
    }

    ::modeleditor_on_event(*impl, e);
}

void osc::Model_editor_screen::tick(float) {
    ::modeleditor_tick(*impl);
}

void osc::Model_editor_screen::draw() {
    gl::ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    osc::ImGuiNewFrame();
    ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode | ImGuiDockNodeFlags_AutoHideTabBar);

    try {
        ::modeleditor_draw_UNGUARDED(*impl);
    } catch (std::exception const& ex) {
        log::error("an OpenSim::Exception was thrown while drawing the editor");
        log::error("    message = %s", ex.what());
        log::error("OpenSim::Exceptions typically happen when the model is damaged or made invalid by an edit (e.g. setting a property to an invalid value)");

        try {
            if (impl->st->can_undo()) {
                log::error("the editor has an `undo` history for this model, so it will try to rollback to that");
                impl->st->edited_model.forcibly_rollback_to_earlier_state();
                log::error("rollback succeeded");
            } else {
                throw;
            }
        } catch (std::exception const& ex2) {
            App::cur().request_transition<Error_screen>(ex2);
        }

        // try to put ImGui into a clean state
        osc::ImGuiShutdown();
        osc::ImGuiInit();
        osc::ImGuiNewFrame();
    }

    osc::ImGuiRender();
}

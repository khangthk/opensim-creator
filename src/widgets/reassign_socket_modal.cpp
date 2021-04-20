#include "reassign_socket_modal.hpp"

#include <OpenSim/Common/Component.h>
#include <OpenSim/Common/ComponentSocket.h>
#include <OpenSim/Simulation/Model/Model.h>
#include <imgui.h>

std::optional<osc::widgets::reassign_socket::Response> osc::widgets::reassign_socket::draw(
    State& st, char const* modal_name, OpenSim::Model const& model, OpenSim::AbstractSocket const&) {

    std::optional<Response> rv;

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

    if (!ImGui::BeginPopupModal(modal_name, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return rv;
    }

    ImGui::InputText("search", st.search, sizeof(st.search));

    ImGui::TextUnformatted("objects:");
    ImGui::BeginChild("obj list", ImVec2(512, 256), true, ImGuiWindowFlags_HorizontalScrollbar);

    for (OpenSim::Component const& c : model.getComponentList()) {
        std::string const& name = c.getName();
        if (name.find(st.search) != std::string::npos) {
            if (ImGui::Selectable(name.c_str())) {
                if (!rv) {
                    rv.emplace(c);
                }
            }
        }
    }
    ImGui::EndChild();

    if (!st.error.empty()) {
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvailWidth());
        ImGui::TextWrapped("%s", st.error.c_str());
    }

    if (ImGui::Button("Cancel")) {
        st.error.clear();
        st.search[0] = '\0';
        ImGui::CloseCurrentPopup();
    }

    ImGui::EndPopup();

    return rv;
}

#include "imgui_demo_screen.hpp"

#include "src/app.hpp"
#include "src/3d/gl.hpp"

#include <imgui.h>

using namespace osc;

void osc::Imgui_demo_screen::on_mount() {
    osc::ImGuiInit();
}

void osc::Imgui_demo_screen::on_unmount() {
    osc::ImGuiShutdown();
}

void Imgui_demo_screen::on_event(SDL_Event const& e) {
    if (osc::ImGuiOnEvent(e)) {
        return;
    }
}

void Imgui_demo_screen::draw() {
    osc::ImGuiNewFrame();

    gl::ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    gl::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    ImGui::ShowDemoWindow();

    osc::ImGuiRender();
}

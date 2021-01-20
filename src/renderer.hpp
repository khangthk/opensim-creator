#pragma once

#include "screen.hpp"
#include "sdl_wrapper.hpp"

#include <glm/vec3.hpp>

#include <memory>

namespace SimTK {
    class State;
}

namespace OpenSim {
    class Model;
}

namespace osmv {
    class Application;

    struct Renderer_private_state;
    struct Renderer final {
        std::unique_ptr<Renderer_private_state> state;

        float radius = 5.0f;
        float theta = 0.88f;
        float phi = 0.4f;
        glm::vec3 pan = {0.3f, -0.5f, 0.0f};
        float fov = 120.0f;
        static constexpr float znear = 0.1f;
        static constexpr float zfar = 100.0f;
        bool dragging = false;
        bool panning = false;
        float sensitivity = 1.0f;
        glm::vec3 light_pos = {1.5f, 3.0f, 0.0f};
        glm::vec3 light_color = {248.0f / 255.0f, 247.0f / 255.0f, 247.0f / 255.0f};
        bool wireframe_mode = false;
        bool gamma_correction = false;
        bool show_mesh_normals = false;
        bool show_floor = true;
        float wheel_sensitivity = 0.9f;

        Renderer();
        Renderer(Renderer const&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer const&) = delete;
        Renderer& operator=(Renderer&&) = delete;
        ~Renderer() noexcept;

        Event_response on_event(Application&, SDL_Event const&);
        void draw(Application const&, OpenSim::Model const&, SimTK::State const&);
    };
}

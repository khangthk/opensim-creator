#pragma once

#include "src/3d/3d.hpp"

#include <SimTKcommon/internal/DecorativeGeometry.h>

#include <utility>
#include <vector>

namespace SimTK {
    class SimbodyMatterSubsystem;
    class State;
}

namespace osc {
    class Simbody_geometry_visitor : public SimTK::DecorativeGeometryImplementation {
        Untextured_mesh& mesh_swap;
        GPU_storage& gpu_cache;
        SimTK::SimbodyMatterSubsystem const& matter_subsys;
        SimTK::State const& state;

    public:
        constexpr Simbody_geometry_visitor(
            Untextured_mesh& _mesh_swap,
            GPU_storage& _cache,
            SimTK::SimbodyMatterSubsystem const& _matter,
            SimTK::State const& _state) noexcept :

            SimTK::DecorativeGeometryImplementation{},

            mesh_swap{_mesh_swap},
            gpu_cache{_cache},
            matter_subsys{_matter},
            state{_state} {
        }

    private:
        virtual void on_instance_created(Mesh_instance const& mi) = 0;

        // implementation details
        void implementPointGeometry(SimTK::DecorativePoint const&) override final;
        void implementLineGeometry(SimTK::DecorativeLine const&) override final;
        void implementBrickGeometry(SimTK::DecorativeBrick const&) override final;
        void implementCylinderGeometry(SimTK::DecorativeCylinder const&) override final;
        void implementCircleGeometry(SimTK::DecorativeCircle const&) override final;
        void implementSphereGeometry(SimTK::DecorativeSphere const&) override final;
        void implementEllipsoidGeometry(SimTK::DecorativeEllipsoid const&) override final;
        void implementFrameGeometry(SimTK::DecorativeFrame const&) override final;
        void implementTextGeometry(SimTK::DecorativeText const&) override final;
        void implementMeshGeometry(SimTK::DecorativeMesh const&) override final;
        void implementMeshFileGeometry(SimTK::DecorativeMeshFile const&) override final;
        void implementArrowGeometry(SimTK::DecorativeArrow const&) override final;
        void implementTorusGeometry(SimTK::DecorativeTorus const&) override final;
        void implementConeGeometry(SimTK::DecorativeCone const&) override final;
    };
}

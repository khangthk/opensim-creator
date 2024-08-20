#include "InMemoryMesh.h"

#include <OpenSimCreator/Utils/OpenSimHelpers.h>
#include <OpenSimCreator/Utils/SimTKHelpers.h>

#include <OpenSim/Simulation/Model/Frame.h>
#include <oscar/Graphics/Scene/SceneDecoration.h>

void osc::mow::InMemoryMesh::implGenerateCustomDecorations(
    const SimTK::State& state,
    const std::function<void(SceneDecoration&&)>& out) const
{
    out(SceneDecoration{
        .mesh = m_OscMesh,
        .transform = decompose_to_transform(getFrame().getTransformInGround(state)),
        .shading = to_color(get_Appearance()),
    });
}

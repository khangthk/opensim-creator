#include "SimTKRenderer.hpp"

#include "src/Graphics/Mesh.hpp"
#include "src/Graphics/MeshCache.hpp"
#include "src/Maths/MathHelpers.hpp"
#include "src/Maths/Segment.hpp"
#include "src/Maths/Triangle.hpp"
#include "src/OpenSimBindings/Rendering/SimTKMeshLoader.hpp"
#include "src/OpenSimBindings/SimTKHelpers.hpp"
#include "src/Platform/Log.hpp"

#include <glm/glm.hpp>
#include <simbody/internal/common.h>
#include <simbody/internal/MobilizedBody.h>
#include <simbody/internal/SimbodyMatterSubsystem.h>
#include <SimTKcommon/internal/DecorativeGeometry.h>
#include <SimTKcommon/internal/State.h>
#include <SimTKcommon/internal/PolygonalMesh.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <utility>
#include <vector>

static inline constexpr float c_LineThickness = 0.005f;
static inline constexpr float c_FrameAxisLengthRescale = 0.25f;
static inline constexpr float c_FrameAxisThickness = 0.0025f;

// helper functions
namespace
{
    // extracts scale factors from geometry
    glm::vec3 GetScaleFactors(SimTK::DecorativeGeometry const& geom)
    {
        SimTK::Vec3 sf = geom.getScaleFactors();

        for (int i = 0; i < 3; ++i)
        {
            sf[i] = sf[i] <= 0 ? 1.0 : sf[i];
        }

        return osc::ToVec3(sf);
    }

    // extracts RGBA from geometry
    glm::vec4 GetColor(SimTK::DecorativeGeometry const& geom)
    {
        SimTK::Vec3 const& rgb = geom.getColor();

        float ar = static_cast<float>(geom.getOpacity());
        ar = ar < 0.0f ? 1.0f : ar;

        return osc::ToVec4(rgb, ar);
    }

    // creates a geometry-to-ground transform for the given geometry
    osc::Transform ToOscTransform(
        SimTK::SimbodyMatterSubsystem const& matter,
        SimTK::State const& state,
        SimTK::DecorativeGeometry const& g)
    {
        SimTK::MobilizedBody const& mobod = matter.getMobilizedBody(SimTK::MobilizedBodyIndex(g.getBodyId()));
        SimTK::Transform const& body2ground = mobod.getBodyTransform(state);
        SimTK::Transform const& decoration2body = g.getTransform();

        osc::Transform rv = osc::ToTransform(body2ground * decoration2body);
        rv.scale = GetScaleFactors(g);

        return rv;
    }
}

// an implementation of SimTK::DecorativeGeometryImplementation that emits generic
// triangle-mesh-based SystemDecorations that can be consumed by the rest of the UI
class osc::SimTKRenderer::Impl final : public SimTK::DecorativeGeometryImplementation {
public:
    Impl(MeshCache& meshCache,
        SimTK::SimbodyMatterSubsystem const& matter,
        SimTK::State const& st,
        float fixupScaleFactor,
        DecorationConsumer& decorationConsumer) :

        m_MeshCache{meshCache},
        m_Matter{matter},
        m_St{st},
        m_FixupScaleFactor{fixupScaleFactor},
        m_Consumer{decorationConsumer}
    {
    }

private:
    Transform ToOscTransform(SimTK::DecorativeGeometry const& d) const
    {
        return ::ToOscTransform(m_Matter, m_St, d);
    }

    void implementPointGeometry(SimTK::DecorativePoint const&) override
    {
        static bool const s_ShownWarningOnce = []()
        {
            log::warn("this model uses implementPointGeometry, which is not yet implemented in OSC");
            return true;
        }();
        (void)s_ShownWarningOnce;
    }

    void implementLineGeometry(SimTK::DecorativeLine const& d) override
    {
        Transform const t = ToOscTransform(d);

        glm::vec3 const p1 = TransformPoint(t, ToVec4(d.getPoint1()));
        glm::vec3 const p2 = TransformPoint(t, ToVec4(d.getPoint2()));

        float const thickness = c_LineThickness * m_FixupScaleFactor;

        Transform cylinderXform = SimbodyCylinderToSegmentTransform({p1, p2}, thickness);
        cylinderXform.scale *= t.scale;

        m_Consumer(m_MeshCache.getCylinderMesh(), cylinderXform, GetColor(d));
    }

    void implementBrickGeometry(SimTK::DecorativeBrick const& d) override
    {
        Transform t = ToOscTransform(d);
        t.scale *= ToVec3(d.getHalfLengths());

        m_Consumer(m_MeshCache.getBrickMesh(), t, GetColor(d));
    }

    void implementCylinderGeometry(SimTK::DecorativeCylinder const& d) override
    {
        float const radius = static_cast<float>(d.getRadius());

        Transform t = ToOscTransform(d);
        t.scale.x *= radius;
        t.scale.y *= static_cast<float>(d.getHalfHeight());
        t.scale.z *= radius;

        m_Consumer(m_MeshCache.getCylinderMesh(), t, GetColor(d));
    }

    void implementCircleGeometry(SimTK::DecorativeCircle const& d) override
    {
        float const radius = static_cast<float>(d.getRadius());

        Transform t = ToOscTransform(d);
        t.scale.x *= radius;
        t.scale.y *= radius;

        m_Consumer(m_MeshCache.getCircleMesh(), t, GetColor(d));
    }

    void implementSphereGeometry(SimTK::DecorativeSphere const& d) override
    {
        Transform t = ToOscTransform(d);
        t.scale *= m_FixupScaleFactor * static_cast<float>(d.getRadius());

        m_Consumer(m_MeshCache.getSphereMesh(), t, GetColor(d));
    }

    void implementEllipsoidGeometry(SimTK::DecorativeEllipsoid const& d) override
    {
        Transform t = ToOscTransform(d);
        t.scale *= ToVec3(d.getRadii());

        m_Consumer(m_MeshCache.getSphereMesh(), t, GetColor(d));
    }

    void implementFrameGeometry(SimTK::DecorativeFrame const& d) override
    {
        Transform const t = ToOscTransform(d);

        // emit origin sphere
        {
            float const radius = 0.05f * c_FrameAxisLengthRescale * m_FixupScaleFactor;
            Transform const sphereXform = t.withScale(radius);
            glm::vec4 const white = {1.0f, 1.0f, 1.0f, 1.0f};

            m_Consumer(m_MeshCache.getSphereMesh(), sphereXform, white);
        }

        // emit leg cylinders
        glm::vec3 const axisLengths = t.scale * static_cast<float>(d.getAxisLength());
        float const legLen = c_FrameAxisLengthRescale * m_FixupScaleFactor;
        float const legThickness = c_FrameAxisThickness * m_FixupScaleFactor;
        for (int axis = 0; axis < 3; ++axis)
        {
            glm::vec3 dir = {0.0f, 0.0f, 0.0f};
            dir[axis] = 1.0f;

            Segment const line =
            {
                t.position,
                t.position + (legLen * axisLengths[axis] * TransformDirection(t, dir))
            };
            Transform const legXform = SimbodyCylinderToSegmentTransform(line, legThickness);

            glm::vec4 color = {0.0f, 0.0f, 0.0f, 1.0f};
            color[axis] = 1.0f;

            m_Consumer(m_MeshCache.getCylinderMesh(), legXform, color);
        }
    }

    void implementTextGeometry(SimTK::DecorativeText const&) override
    {
        static bool const s_ShownWarningOnce = []()
        {
            log::warn("this model uses implementTextGeometry, which is not yet implemented in OSC");
            return true;
        }();
        (void)s_ShownWarningOnce;
    }

    void implementMeshGeometry(SimTK::DecorativeMesh const& d) override
    {
        // roughly based on simbody's VisualizerProtocol.cpp:drawPolygonalMesh
        //
        // (it uses impl pointers to figure out mesh caching - unsure if that's a
        //  good idea, but it is reproduced here for consistency)

        std::string const id = std::to_string(reinterpret_cast<uintptr_t>(static_cast<void const*>(&d.getMesh().getImpl())));
        auto const meshLoaderFunc = [&d]() { return ToOscMesh(d.getMesh()); };

        m_Consumer(m_MeshCache.get(id, meshLoaderFunc), ToOscTransform(d), GetColor(d));
    }

    void implementMeshFileGeometry(SimTK::DecorativeMeshFile const& d) override
    {
        std::string const& path = d.getMeshFile();
        auto meshLoader = [&path](){ return LoadMeshViaSimTK(path); };
        Mesh const mesh = m_MeshCache.get(d.getMeshFile(), meshLoader);

        m_Consumer(mesh, ToOscTransform(d), GetColor(d));
    }

    void implementArrowGeometry(SimTK::DecorativeArrow const& d) override
    {
        Transform const t = ToOscTransform(d);

        glm::vec3 const startBase = ToVec3(d.getStartPoint());
        glm::vec3 const endBase = ToVec3(d.getEndPoint());

        glm::vec3 const start = TransformPoint(t, startBase);
        glm::vec3 const end = TransformPoint(t, endBase);

        glm::vec3 const dir = glm::normalize(end - start);

        glm::vec3 const neckStart = start;
        glm::vec3 const neckEnd = end - (static_cast<float>(d.getTipLength()) * dir);
        glm::vec3 const headStart = neckEnd;
        glm::vec3 const headEnd = end;

        constexpr float neckThickness = 0.005f;
        constexpr float headThickness = 0.02f;

        glm::vec4 const color = GetColor(d);

        // emit neck cylinder
        Transform const neckXform = SimbodyCylinderToSegmentTransform({neckStart, neckEnd}, neckThickness);
        m_Consumer(m_MeshCache.getCylinderMesh(), neckXform, color);

        // emit head cone
        Transform const headXform = SimbodyCylinderToSegmentTransform({headStart, headEnd}, headThickness);
        m_Consumer(m_MeshCache.getConeMesh(), headXform, color);
    }

    void implementTorusGeometry(SimTK::DecorativeTorus const& d) override
    {
        float const torusCenterToTubeCenterRadius = static_cast<float>(d.getTorusRadius());
        float const tubeRadius = static_cast<float>(d.getTubeRadius());

        m_Consumer(m_MeshCache.getTorusMesh(torusCenterToTubeCenterRadius, tubeRadius), ToOscTransform(d), GetColor(d));
    }

    void implementConeGeometry(SimTK::DecorativeCone const& d) override
    {
        Transform const t = ToOscTransform(d);

        glm::vec3 const posBase = ToVec3(d.getOrigin());
        glm::vec3 const posDir = ToVec3(d.getDirection());

        glm::vec3 const pos = TransformPoint(t, posBase);
        glm::vec3 const dir = TransformDirection(t, posDir);

        float const radius = static_cast<float>(d.getBaseRadius());
        float const height = static_cast<float>(d.getHeight());

        Transform coneXform = SimbodyCylinderToSegmentTransform({pos, pos + height*dir}, radius);
        coneXform.scale *= t.scale;

        m_Consumer(m_MeshCache.getConeMesh(), coneXform, GetColor(d));
    }

    MeshCache& m_MeshCache;
    SimTK::SimbodyMatterSubsystem const& m_Matter;
    SimTK::State const& m_St;
    float m_FixupScaleFactor;
    DecorationConsumer& m_Consumer;
};

// osc::DecorativeGeometryHandler

osc::SimTKRenderer::SimTKRenderer(
    MeshCache& meshCache,
    SimTK::SimbodyMatterSubsystem const& matter,
    SimTK::State const& state,
    float fixupScaleFactor,
    DecorationConsumer& decorationConsumer) :

    m_Impl{std::make_unique<Impl>(
        meshCache,
        matter,
        state,
        fixupScaleFactor,
        decorationConsumer)}
{
}

osc::SimTKRenderer::SimTKRenderer(SimTKRenderer&&) noexcept = default;
osc::SimTKRenderer& osc::SimTKRenderer::operator=(SimTKRenderer&&) noexcept = default;
osc::SimTKRenderer::~SimTKRenderer() noexcept = default;

void osc::SimTKRenderer::operator()(SimTK::DecorativeGeometry const& dg)
{
    dg.implementGeometry(*m_Impl);
}

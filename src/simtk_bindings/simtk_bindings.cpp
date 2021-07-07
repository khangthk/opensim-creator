#include "simtk_bindings.hpp"

#include "src/3d/3d.hpp"
#include "src/constants.hpp"
#include "src/log.hpp"

#include <SimTKcommon/Orientation.h>
#include <glm/ext/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>
#include <simbody/internal/MobilizedBody.h>
#include <simbody/internal/SimbodyMatterSubsystem.h>

#include <algorithm>
#include <limits>

using namespace SimTK;
using namespace osc;

// create an xform that transforms the unit cylinder into a line between
// two points
static glm::mat4 cylinder_to_line_xform(float line_width, glm::vec3 const& p1, glm::vec3 const& p2) {
    // P1 -> P2
    glm::vec3 p1_to_p2 = p2 - p1;

    // cylinder bottom -> cylinder top
    //
    // defined to be 2.0f in Y (by the known design of the cylinder mesh instance)
    constexpr glm::vec3 cbot_to_ctop = glm::vec3{0.0f, 2.0f, 0.0f};

    // our goal is to compute a transform that transforms the unit cylinder's
    // top-to-bottom vector such that it aligns along P1 -> P2. This is so that
    // the same (instanced) cylinder mesh can be reused by just applying this
    // transform

    glm::vec3 perpendicular_axis = glm::cross(glm::normalize(cbot_to_ctop), glm::normalize(p1_to_p2));

    // rotate C_bot -> C_top to be parallel to P1 -> P2
    glm::mat4 rotation_xform;
    if (glm::length(perpendicular_axis) == 0.0f) {
        rotation_xform = glm::identity<glm::mat4>();  // already aligned
    } else {
        float cos_angle = glm::dot(glm::normalize(cbot_to_ctop), glm::normalize(p1_to_p2));
        float angle = glm::acos(cos_angle);
        rotation_xform = glm::rotate(glm::identity<glm::mat4>(), angle, perpendicular_axis);
    }

    // scale C_bot -> C_top to be equal to P1 -> P2
    float line_length_scale = glm::length(p1_to_p2) / glm::length(cbot_to_ctop);
    glm::vec3 scale_amt = {line_width, line_length_scale, line_width};
    glm::mat4 scale_xform = glm::scale(glm::identity<glm::mat4>(), scale_amt);

    // translate cylinder origin (0, 0) to P1 -> P2 origin
    glm::mat4 translation_xform = glm::translate(glm::mat4{1.0f}, p1 + p1_to_p2/2.0f);

    // scale it around origin, rotate it around origin, then translate to correct location
    return translation_xform * rotation_xform * scale_xform;
}

// load a SimTK::PolygonalMesh into an osc::Untextured_vert mesh ready for GPU upload
static void load_mesh_data(PolygonalMesh const& mesh, Untextured_mesh& out) {

    // helper function: gets a vertex for a face
    auto get_face_vert_pos = [&](int face, int vert) {
        SimTK::Vec3 pos = mesh.getVertexPosition(mesh.getFaceVertex(face, vert));
        return glm::vec3{pos[0], pos[1], pos[2]};
    };

    // helper function: compute the normal of the triangle p1, p2, p3
    auto make_normal = [](glm::vec3 const& p1, glm::vec3 const& p2, glm::vec3 const& p3) {
        return glm::normalize(glm::cross(p2 - p1, p3 - p1));
    };

    out.clear();
    std::vector<Untextured_vert>& triangles = out.verts;

    // iterate over each face in the PolygonalMesh and transform each into a sequence of
    // GPU-friendly triangle verts
    for (auto face = 0; face < mesh.getNumFaces(); ++face) {
        auto num_vertices = mesh.getNumVerticesForFace(face);

        if (num_vertices < 3) {
            // line?: ignore for now
        } else if (num_vertices == 3) {
            // triangle: use as-is
            glm::vec3 p1 = get_face_vert_pos(face, 0);
            glm::vec3 p2 = get_face_vert_pos(face, 1);
            glm::vec3 p3 = get_face_vert_pos(face, 2);
            glm::vec3 normal = make_normal(p1, p2, p3);

            triangles.push_back({p1, normal});
            triangles.push_back({p2, normal});
            triangles.push_back({p3, normal});
        } else if (num_vertices == 4) {
            // quad: split into two triangles

            glm::vec3 p1 = get_face_vert_pos(face, 0);
            glm::vec3 p2 = get_face_vert_pos(face, 1);
            glm::vec3 p3 = get_face_vert_pos(face, 2);
            glm::vec3 p4 = get_face_vert_pos(face, 3);

            glm::vec3 t1_norm = make_normal(p1, p2, p3);
            glm::vec3 t2_norm = make_normal(p3, p4, p1);

            triangles.push_back({p1, t1_norm});
            triangles.push_back({p2, t1_norm});
            triangles.push_back({p3, t1_norm});

            triangles.push_back({p3, t2_norm});
            triangles.push_back({p4, t2_norm});
            triangles.push_back({p1, t2_norm});
        } else {
            // polygon (>3 edges):
            //
            // create a vertex at the average center point and attach
            // every two verices to the center as triangles.

            auto center = glm::vec3{0.0f, 0.0f, 0.0f};
            for (int vert = 0; vert < num_vertices; ++vert) {
                center += get_face_vert_pos(face, vert);
            }
            center /= num_vertices;

            for (int vert = 0; vert < num_vertices - 1; ++vert) {
                glm::vec3 p1 = get_face_vert_pos(face, vert);
                glm::vec3 p2 = get_face_vert_pos(face, vert + 1);
                glm::vec3 normal = make_normal(p1, p2, center);

                triangles.push_back({p1, normal});
                triangles.push_back({p2, normal});
                triangles.push_back({center, normal});
            }

            // complete the polygon loop
            glm::vec3 p1 = get_face_vert_pos(face, num_vertices - 1);
            glm::vec3 p2 = get_face_vert_pos(face, 0);
            glm::vec3 normal = make_normal(p1, p2, center);

            triangles.push_back({p1, normal});
            triangles.push_back({p2, normal});
            triangles.push_back({center, normal});
        }
    }

    generate_1to1_indices_for_verts(out);
}

void osc::load_mesh_file_with_simtk_backend(std::filesystem::path const& p, Untextured_mesh& out) {
    SimTK::DecorativeMeshFile dmf{p.string()};
    load_mesh_data(dmf.getMesh(), out);
}

static Transform ground_to_decoration_xform(
    SimbodyMatterSubsystem const& ms, SimTK::State const& state, DecorativeGeometry const& geom) {
    MobilizedBody const& mobod = ms.getMobilizedBody(MobilizedBodyIndex(geom.getBodyId()));
    Transform const& ground_to_body_xform = mobod.getBodyTransform(state);
    Transform const& body_to_decoration_xform = geom.getTransform();

    return ground_to_body_xform * body_to_decoration_xform;
}

glm::mat4 osc::to_mat4(Transform const& t) noexcept {
    // glm::mat4 is column major:
    //     see: https://glm.g-truc.net/0.9.2/api/a00001.html
    //     (and just Google "glm column major?")
    //
    // SimTK is row-major, carefully read the sourcecode for
    // `SimTK::Transform`.

    glm::mat4 m;

    // x0 y0 z0 w0
    Rotation const& r = t.R();
    Vec3 const& p = t.p();

    {
        auto const& row0 = r[0];
        m[0][0] = static_cast<float>(row0[0]);
        m[1][0] = static_cast<float>(row0[1]);
        m[2][0] = static_cast<float>(row0[2]);
        m[3][0] = static_cast<float>(p[0]);
    }

    {
        auto const& row1 = r[1];
        m[0][1] = static_cast<float>(row1[0]);
        m[1][1] = static_cast<float>(row1[1]);
        m[2][1] = static_cast<float>(row1[2]);
        m[3][1] = static_cast<float>(p[1]);
    }

    {
        auto const& row2 = r[2];
        m[0][2] = static_cast<float>(row2[0]);
        m[1][2] = static_cast<float>(row2[1]);
        m[2][2] = static_cast<float>(row2[2]);
        m[3][2] = static_cast<float>(p[2]);
    }

    m[0][3] = 0.0f;
    m[1][3] = 0.0f;
    m[2][3] = 0.0f;
    m[3][3] = 1.0f;

    return m;
}

SimTK::Transform osc::to_transform(glm::mat4 const& m) noexcept {
    // glm::mat4 is column-major, SimTK::Transform is effectively
    // row-major

    SimTK::Mat33 mtx{
        m[0][0], m[1][0], m[2][0],
        m[0][1], m[1][1], m[2][1],
        m[0][2], m[1][2], m[2][2],
    };
    SimTK::Vec3 translation{m[3][0], m[3][1], m[3][2]};

    SimTK::Rotation rot{mtx};

    return SimTK::Transform{rot, translation};
}

static glm::mat4 geom_to_mat4(SimbodyMatterSubsystem const& ms, SimTK::State const& state, DecorativeGeometry const& geom) {
    return to_mat4(ground_to_decoration_xform(ms, state, geom));
}

static glm::vec3 scale_factors(DecorativeGeometry const& geom) {
    Vec3 sf = geom.getScaleFactors();
    for (int i = 0; i < 3; ++i) {
        sf[i] = sf[i] <= 0 ? 1.0 : sf[i];
    }
    return glm::vec3{sf[0], sf[1], sf[2]};
}

static Rgba32 extract_rgba(DecorativeGeometry const& geom) {
    Vec3 const& rgb = geom.getColor();
    Real ar = geom.getOpacity();
    ar = ar < 0.0 ? 1.0 : ar;

    return Rgba32::from_d4(rgb[0], rgb[1], rgb[2], ar);
}

static glm::vec4 to_vec4(Vec3 const& v, float w = 1.0f) {
    return glm::vec4{v[0], v[1], v[2], w};
}

void Simbody_geometry_visitor::implementPointGeometry(SimTK::DecorativePoint const&) {
    static bool shown_nyi_warning = []() {
        log::warn("this model uses implementPointGeometry, which is not yet implemented in OSC");
        return true;
    }();
    (void)shown_nyi_warning;
}

void Simbody_geometry_visitor::implementLineGeometry(SimTK::DecorativeLine const& geom) {
    glm::mat4 xform = geom_to_mat4(matter_subsys, state, geom);
    glm::vec3 p1 = xform * to_vec4(geom.getPoint1());
    glm::vec3 p2 = xform * to_vec4(geom.getPoint2());

    Mesh_instance mi;
    mi.model_xform = cylinder_to_line_xform(0.005f, p1, p2);
    mi.normal_xform = normal_matrix(mi.model_xform);
    mi.rgba = extract_rgba(geom);
    mi.meshidx = gpu_cache.simbody_cylinder_idx;
    on_instance_created(mi);
}

void Simbody_geometry_visitor::implementBrickGeometry(SimTK::DecorativeBrick const& geom) {
    SimTK::Vec3 dims = geom.getHalfLengths();
    glm::mat4 base_xform = geom_to_mat4(matter_subsys, state, geom);

    Mesh_instance mi;
    mi.model_xform = glm::scale(base_xform, glm::vec3{dims[0], dims[1], dims[2]});
    mi.normal_xform = normal_matrix(mi.model_xform);
    mi.rgba = extract_rgba(geom);
    mi.meshidx = gpu_cache.simbody_cube_idx;
    on_instance_created(mi);
}

void Simbody_geometry_visitor::implementCylinderGeometry(SimTK::DecorativeCylinder const& geom) {
    glm::mat4 m = geom_to_mat4(matter_subsys, state, geom);
    glm::vec3 s = scale_factors(geom);
    s.x *= static_cast<float>(geom.getRadius());
    s.y *= static_cast<float>(geom.getHalfHeight());
    s.z *= static_cast<float>(geom.getRadius());

    Mesh_instance mi;
    mi.model_xform = glm::scale(m, s);
    mi.normal_xform = normal_matrix(mi.model_xform);
    mi.rgba = extract_rgba(geom);
    mi.meshidx = gpu_cache.simbody_cylinder_idx;
    on_instance_created(mi);
}

void Simbody_geometry_visitor::implementCircleGeometry(SimTK::DecorativeCircle const&) {
    static bool shown_nyi_warning = []() {
        log::warn("this model uses implementCircleGeometry, which is not yet implemented in OSC");
        return true;
    }();
    (void)shown_nyi_warning;
}

void Simbody_geometry_visitor::implementSphereGeometry(SimTK::DecorativeSphere const& geom) {
    float r = static_cast<float>(geom.getRadius());

    Mesh_instance mi;
    mi.model_xform = glm::scale(geom_to_mat4(matter_subsys, state, geom), glm::vec3{r, r, r});
    mi.normal_xform = normal_matrix(mi.model_xform);
    mi.rgba = extract_rgba(geom);
    mi.meshidx = gpu_cache.simbody_sphere_idx;
    on_instance_created(mi);
}

void Simbody_geometry_visitor::implementEllipsoidGeometry(SimTK::DecorativeEllipsoid const&) {
    static bool shown_nyi_warning = []() {
        log::warn("this model uses implementEllipsoidGeometry, which is not yet implemented in OSC");
        return true;
    }();
    (void)shown_nyi_warning;
}

void Simbody_geometry_visitor::implementFrameGeometry(SimTK::DecorativeFrame const& geom) {
    glm::mat4 xform = geom_to_mat4(matter_subsys, state, geom);

    glm::mat4 scaler = [&geom]() {
        glm::vec3 s = scale_factors(geom);
        s *= static_cast<float>(geom.getAxisLength());
        return glm::scale(glm::identity<glm::mat4>(), glm::vec3{0.015f * s.x, 0.1f * s.y, 0.015f * s.z});
    }();

    glm::mat4 mover = glm::translate(glm::identity<glm::mat4>(), glm::vec3{0.0f, 1.0f, 0.0f});


    // origin
    {
        Mesh_instance origin;
        origin.model_xform = glm::scale(xform, glm::vec3{0.0075f});
        origin.normal_xform = normal_matrix(origin.model_xform);
        origin.rgba = {0xff, 0xff, 0xff, 0xff};
        origin.meshidx = gpu_cache.simbody_sphere_idx;
        on_instance_created(origin);
    }

    // y axis
    {
        Mesh_instance y;
        y.model_xform = xform * scaler * mover;
        y.normal_xform = normal_matrix(y.model_xform);
        y.rgba = {0x00, 191, 0x00, 0xff};
        y.meshidx = gpu_cache.simbody_cylinder_idx;
        on_instance_created(y);
    }

    // x axis
    {
        glm::mat4 rotate_plusy_to_plusx =
            glm::rotate(glm::identity<glm::mat4>(), pi_f / 2.0f, glm::vec3{0.0f, 0.0f, -1.0f});

        Mesh_instance x;
        x.model_xform = xform * rotate_plusy_to_plusx * scaler * mover;
        x.normal_xform = normal_matrix(x.model_xform);
        x.rgba = {191, 0x00, 0x00, 0xff};
        x.meshidx = gpu_cache.simbody_cylinder_idx;
        on_instance_created(x);
    }

    // z axis
    {
        glm::mat4 rotate_plusy_to_plusz = glm::rotate(glm::identity<glm::mat4>(), pi_f / 2.0f, glm::vec3{1.0f, 0.0f, 0.0f});
        Mesh_instance z;
        z.model_xform = xform * rotate_plusy_to_plusz * scaler * mover;
        z.normal_xform = normal_matrix(z.model_xform);
        z.rgba = {0x00, 0x00, 191, 0xff};
        z.meshidx = gpu_cache.simbody_cylinder_idx;
        on_instance_created(z);
    }
}

void Simbody_geometry_visitor::implementTextGeometry(SimTK::DecorativeText const&) {
    static bool shown_nyi_warning = []() {
        log::warn("this model uses implementTextGeometry, which is not yet implemented in OSC");
        return true;
    }();
    (void)shown_nyi_warning;
}

void Simbody_geometry_visitor::implementMeshGeometry(SimTK::DecorativeMesh const&) {
    static bool shown_nyi_warning = []() {
        log::warn("this model uses implementMeshGeometry, which is not yet implemented in OSC");
        return true;
    }();
    (void)shown_nyi_warning;
}

void Simbody_geometry_visitor::implementMeshFileGeometry(SimTK::DecorativeMeshFile const& geom) {
    auto [it, inserted] = gpu_cache.path_to_meshidx.emplace(geom.getMeshFile(), Meshidx{});

    if (inserted) {
        // cache miss: go load the mesh
        load_mesh_data(geom.getMesh(), mesh_swap);
        gpu_cache.meshes.emplace_back(mesh_swap);
        it->second = Meshidx::from_index(gpu_cache.meshes.size() - 1);
    }

    Mesh_instance mi;
    mi.model_xform = glm::scale(geom_to_mat4(matter_subsys, state, geom), scale_factors(geom));
    mi.normal_xform = normal_matrix(mi.model_xform);
    mi.rgba = extract_rgba(geom);
    mi.meshidx = it->second;
    on_instance_created(mi);
}

void Simbody_geometry_visitor::implementArrowGeometry(SimTK::DecorativeArrow const&) {
    static bool shown_nyi_warning = []() {
        log::warn("this model uses implementArrowGeometry, which is not yet implemented in OSC");
        return true;
    }();
    (void)shown_nyi_warning;
}

void Simbody_geometry_visitor::implementTorusGeometry(SimTK::DecorativeTorus const&) {
    static bool shown_nyi_warning = []() {
        log::warn("this model uses implementTorusGeometry, which is not yet implemented in OSC");
        return true;
    }();
    (void)shown_nyi_warning;
}

void Simbody_geometry_visitor::implementConeGeometry(SimTK::DecorativeCone const&) {
    static bool shown_nyi_warning = []() {
        log::warn("this model uses implementConeGeometry, which is not yet implemented in OSC");
        return true;
    }();
    (void)shown_nyi_warning;
}

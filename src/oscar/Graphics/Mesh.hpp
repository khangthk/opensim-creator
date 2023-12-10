#pragma once

#include <oscar/Graphics/MeshIndicesView.hpp>
#include <oscar/Graphics/MeshTopology.hpp>
#include <oscar/Maths/Mat4.hpp>
#include <oscar/Maths/Transform.hpp>
#include <oscar/Maths/Vec2.hpp>
#include <oscar/Maths/Vec3.hpp>
#include <oscar/Maths/Vec4.hpp>
#include <oscar/Utils/CopyOnUpdPtr.hpp>

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <span>
#include <vector>

namespace osc { struct AABB; }
namespace osc { struct Color; }
namespace osc { class SubMeshDescriptor; }
namespace osc { struct Transform; }

// note: implementation is in `GraphicsImplementation.cpp`
namespace osc
{
    // mesh
    //
    // encapsulates mesh data, which may include vertices, indices, normals, texture
    // coordinates, etc.
    class Mesh final {
    public:
        Mesh();

        MeshTopology getTopology() const;
        void setTopology(MeshTopology);

        bool hasVertexData() const;
        size_t getNumVerts() const;

        std::span<Vec3 const> getVerts() const;
        void setVerts(std::span<Vec3 const>);
        void transformVerts(std::function<void(std::span<Vec3>)> const&);
        void transformVerts(Transform const&);
        void transformVerts(Mat4 const&);

        std::span<Vec3 const> getNormals() const;
        void setNormals(std::span<Vec3 const>);
        void transformNormals(std::function<void(std::span<Vec3>)> const&);

        std::span<Vec2 const> getTexCoords() const;
        void setTexCoords(std::span<Vec2 const>);
        void transformTexCoords(std::function<void(std::span<Vec2>)> const&);

        std::span<Color const> getColors() const;
        void setColors(std::span<Color const>);

        std::span<Vec4 const> getTangents() const;
        void setTangents(std::span<Vec4 const>);

        MeshIndicesView getIndices() const;
        void setIndices(MeshIndicesView);
        void setIndices(std::span<uint16_t const>);
        void setIndices(std::span<uint32_t const>);

        AABB const& getBounds() const;  // local-space

        void clear();

        size_t getSubMeshCount() const;
        void pushSubMeshDescriptor(SubMeshDescriptor const&);
        SubMeshDescriptor const& getSubMeshDescriptor(size_t) const;
        void clearSubMeshDescriptors();

        friend void swap(Mesh& a, Mesh& b) noexcept
        {
            swap(a.m_Impl, b.m_Impl);
        }

        friend bool operator==(Mesh const&, Mesh const&) = default;
        friend std::ostream& operator<<(std::ostream&, Mesh const&);
    private:
        friend class GraphicsBackend;
        friend struct std::hash<Mesh>;

        class Impl;
        CopyOnUpdPtr<Impl> m_Impl;
    };

    std::ostream& operator<<(std::ostream&, Mesh const&);
}

template<>
struct std::hash<osc::Mesh> final {
    size_t operator()(osc::Mesh const& mesh) const
    {
        return std::hash<osc::CopyOnUpdPtr<osc::Mesh::Impl>>{}(mesh.m_Impl);
    }
};

﻿#pragma once

#include "src/3D/BVH.hpp"
#include "src/3D/Gl.hpp"
#include "src/3D/Model.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <nonstd/span.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

namespace osc {

    enum class IndexFormat {
        UInt16,
        UInt32,
    };

    class Mesh {
    public:
        struct Impl;
    private:
        std::unique_ptr<Impl> m_Impl;

    public:
        using IdType = int64_t;

        Mesh(MeshData);
        Mesh(Mesh const&) = delete;
        Mesh(Mesh&&) noexcept;
        ~Mesh() noexcept;

        Mesh& operator=(Mesh const&) = delete;
        Mesh& operator=(Mesh&&) noexcept;

        MeshTopography getTopography() const;
        GLenum getTopographyOpenGL() const;
        void setTopography(MeshTopography);

        nonstd::span<glm::vec3 const> getVerts() const;
        void setVerts(nonstd::span<glm::vec3 const>);

        nonstd::span<glm::vec3 const> getNormals() const;
        void setNormals(nonstd::span<glm::vec3 const>);

        nonstd::span<glm::vec2 const> getTexCoords() const;
        void setTexCoords(nonstd::span<glm::vec2 const>);
        void scaleTexCoords(float);

        IndexFormat getIndexFormat() const;
        GLenum getIndexFormatOpenGL() const;
        void setIndexFormat(IndexFormat);

        int getNumIndices() const;
        std::vector<uint32_t> getIndices() const;  // note: copies them, because IndexFormat may be U16 internally
        void setIndicesU16(nonstd::span<uint16_t const>);
        void setIndicesU32(nonstd::span<uint32_t const>);  // note: format trumps this, value will be truncated

        AABB const& getAABB() const;

        Sphere const& getBoundingSphere() const;

        BVH const& getTriangleBVH() const;

        // returns !hit if the line doesn't intersect it *or* the
        // topography is not triangulars
        RayCollision getClosestRayTriangleCollision(Line const&) const;

        void clear();
        void recalculateBounds();
        void uploadToGPU();  // must be called from GPU thread

        gl::VertexArray& GetVertexArray();
        void Draw();
        void DrawInstanced(size_t n);
    };
}

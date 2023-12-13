#include <oscar/Graphics/Mesh.hpp>

#include <testoscar/TestingHelpers.hpp>

#include <gtest/gtest.h>
#include <oscar/Graphics/Color.hpp>
#include <oscar/Graphics/MeshTopology.hpp>
#include <oscar/Graphics/SubMeshDescriptor.hpp>
#include <oscar/Maths/AABB.hpp>
#include <oscar/Maths/Mat4.hpp>
#include <oscar/Maths/MathHelpers.hpp>
#include <oscar/Maths/Quat.hpp>
#include <oscar/Maths/Transform.hpp>
#include <oscar/Maths/Vec2.hpp>
#include <oscar/Maths/Vec3.hpp>
#include <oscar/Maths/Vec4.hpp>
#include <oscar/Utils/Concepts.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <random>
#include <span>
#include <sstream>
#include <utility>
#include <vector>

using osc::testing::GenerateColors;
using osc::testing::GenerateNormals;
using osc::testing::GenerateTangents;
using osc::testing::GenerateTexCoords;
using osc::testing::GenerateTriangleVerts;
using osc::testing::GenerateVec2;
using osc::testing::GenerateVec3;
using osc::testing::GenerateVertices;
using osc::testing::MapToVector;
using osc::testing::ResizedVectorCopy;
using osc::AABB;
using osc::AABBFromVerts;
using osc::Color;
using osc::ContiguousContainer;
using osc::Deg2Rad;
using osc::Identity;
using osc::Mat4;
using osc::Mesh;
using osc::MeshTopology;
using osc::Quat;
using osc::SubMeshDescriptor;
using osc::ToMat4;
using osc::Transform;
using osc::TransformPoint;
using osc::Vec2;
using osc::Vec3;
using osc::Vec4;

TEST(Mesh, CanBeDefaultConstructed)
{
    Mesh const mesh;
}

TEST(Mesh, CanBeCopyConstructed)
{
    Mesh const m;
    Mesh{m};
}

TEST(Mesh, CanBeMoveConstructed)
{
    Mesh m1;
    Mesh const m2{std::move(m1)};
}

TEST(Mesh, CanBeCopyAssigned)
{
    Mesh m1;
    Mesh const m2;

    m1 = m2;
}

TEST(Mesh, CanBeMoveAssigned)
{
    Mesh m1;
    Mesh m2;

    m1 = std::move(m2);
}

TEST(Mesh, CanGetTopology)
{
    Mesh const m;

    m.getTopology();
}

TEST(Mesh, GetTopologyDefaultsToTriangles)
{
    Mesh const m;

    ASSERT_EQ(m.getTopology(), MeshTopology::Triangles);
}

TEST(Mesh, SetTopologyCausesGetTopologyToUseSetValue)
{
    Mesh m;
    auto const newTopology = MeshTopology::Lines;

    ASSERT_NE(m.getTopology(), MeshTopology::Lines);

    m.setTopology(newTopology);

    ASSERT_EQ(m.getTopology(), newTopology);
}

TEST(Mesh, SetTopologyCausesCopiedMeshTobeNotEqualToInitialMesh)
{
    Mesh const m;
    Mesh copy{m};
    auto const newTopology = MeshTopology::Lines;

    ASSERT_EQ(m, copy);
    ASSERT_NE(copy.getTopology(), newTopology);

    copy.setTopology(newTopology);

    ASSERT_NE(m, copy);
}

TEST(Mesh, GetNumVertsInitiallyEmpty)
{
    ASSERT_EQ(Mesh{}.getNumVerts(), 0);
}

TEST(Mesh, Assigning3VertsMakesGetNumVertsReturn3Verts)
{
    Mesh m;
    m.setVerts(GenerateVertices(3));
    ASSERT_EQ(m.getNumVerts(), 3);
}

TEST(Mesh, HasVertsInitiallyfalse)
{
    ASSERT_FALSE(Mesh{}.hasVerts());
}

TEST(Mesh, HasVertsTrueAfterSettingVerts)
{
    Mesh m;
    m.setVerts(GenerateVertices(6));
    ASSERT_TRUE(m.hasVerts());
}

TEST(Mesh, GetVertsReturnsEmptyVertsOnDefaultConstruction)
{
    ASSERT_TRUE(Mesh{}.getVerts().empty());
}

TEST(Mesh, SetVertsMakesGetCallReturnVerts)
{
    Mesh m;
    auto const verts = GenerateVertices(9);

    m.setVerts(verts);

    ASSERT_EQ(m.getVerts(), verts);
}

TEST(Mesh, SetVertsCausesCopiedMeshToNotBeEqualToInitialMesh)
{
    Mesh const m;
    Mesh copy{m};

    ASSERT_EQ(m, copy);

    copy.setVerts(GenerateVertices(30));

    ASSERT_NE(m, copy);
}

TEST(Mesh, ShrinkingVertsCausesNormalsToShrinkAlso)
{
    auto const normals = GenerateNormals(6);

    Mesh m;
    m.setVerts(GenerateVertices(6));
    m.setNormals(normals);
    m.setVerts(GenerateVertices(3));

    ASSERT_EQ(m.getNormals(), ResizedVectorCopy(normals, 3));
}

TEST(Mesh, ExpandingVertsCausesNormalsToExpandWithZeroedNormals)
{
    auto const normals = GenerateNormals(6);

    Mesh m;
    m.setVerts(GenerateVertices(6));
    m.setNormals(normals);
    m.setVerts(GenerateVertices(12));

    ASSERT_EQ(m.getNormals(), ResizedVectorCopy(normals, 12, Vec3{}));
}

TEST(Mesh, ShrinkingVertsCausesTexCoordsToShrinkAlso)
{
    auto uvs = GenerateTexCoords(6);

    Mesh m;
    m.setVerts(GenerateVertices(6));
    m.setTexCoords(uvs);
    m.setVerts(GenerateVertices(3));

    ASSERT_EQ(m.getTexCoords(), ResizedVectorCopy(uvs, 3));
}

TEST(Mesh, ExpandingVertsCausesTexCoordsToExpandWithZeroedTexCoords)
{
    auto const uvs = GenerateTexCoords(6);

    Mesh m;
    m.setVerts(GenerateVertices(6));
    m.setTexCoords(uvs);
    m.setVerts(GenerateVertices(12));

    ASSERT_EQ(m.getTexCoords(), ResizedVectorCopy(uvs, 12, Vec2{}));
}

TEST(Mesh, ShrinkingVertsCausesColorsToShrinkAlso)
{
    auto const colors = GenerateColors(6);

    Mesh m;
    m.setVerts(GenerateVertices(6));
    m.setColors(colors);
    m.setVerts(GenerateVertices(3));

    ASSERT_EQ(m.getColors(), ResizedVectorCopy(colors, 3));
}

TEST(Mesh, ExpandingVertsCausesColorsToExpandWithClearColor)
{
    auto const colors = GenerateColors(6);

    Mesh m;
    m.setVerts(GenerateVertices(6));
    m.setColors(colors);
    m.setVerts(GenerateVertices(12));

    ASSERT_EQ(m.getColors(), ResizedVectorCopy(colors, 12, Color::clear()));
}

TEST(Mesh, ShrinkingVertsCausesTangentsToShrinkAlso)
{
    auto const tangents = GenerateTangents(6);

    Mesh m;
    m.setVerts(GenerateVertices(6));
    m.setTangents(GenerateTangents(6));
    m.setVerts(GenerateVertices(3));

    ASSERT_EQ(m.getTangents(), ResizedVectorCopy(tangents, 3));
}

TEST(Mesh, ExpandingVertsCausesTangentsToExpandAlsoAsZeroedTangents)
{
    auto const tangents = GenerateTangents(6);

    Mesh m;
    m.setVerts(GenerateVertices(6));
    m.setTangents(GenerateTangents(6));
    m.setVerts(GenerateVertices(12));

    ASSERT_EQ(m.getTangents(), ResizedVectorCopy(tangents, 12, Vec4{}));
}

TEST(Mesh, TransformVertsMakesGetCallReturnVerts)
{
    Mesh m;

    // generate "original" verts
    auto const originalVerts = GenerateVertices(30);

    // create "transformed" version of the verts
    auto const newVerts = MapToVector(originalVerts, [](auto const& v) { return v + 1.0f; });

    // sanity check that `setVerts` works as expected
    ASSERT_FALSE(m.hasVerts());
    m.setVerts(originalVerts);
    ASSERT_EQ(m.getVerts(), originalVerts);

    // the verts passed to `transformVerts` should match those returned by getVerts
    std::vector<Vec3> vertsPassedToTransformVerts;
    m.transformVerts([&vertsPassedToTransformVerts](Vec3& v) { vertsPassedToTransformVerts.push_back(v); });
    ASSERT_EQ(vertsPassedToTransformVerts, originalVerts);

    // applying the transformation should return the transformed verts
    m.transformVerts([&newVerts, i = 0](Vec3& v) mutable
    {
        v = newVerts.at(i++);
    });
    ASSERT_EQ(m.getVerts(), newVerts);
}

TEST(Mesh, TransformVertsCausesTransformedMeshToNotBeEqualToInitialMesh)
{
    Mesh const m;
    Mesh copy{m};

    ASSERT_EQ(m, copy);

    copy.transformVerts([](Vec3&) {});  // noop transform also triggers this (meshes aren't value-comparable)

    ASSERT_NE(m, copy);
}

TEST(Mesh, TransformVertsWithTransformAppliesTransformToVerts)
{
    // create appropriate transform
    Transform const transform = {
        .scale = Vec3{0.25f},
        .rotation = Quat{Vec3{Deg2Rad(90.0f), 0.0f, 0.0f}},
        .position = {1.0f, 0.25f, 0.125f},
    };

    // generate "original" verts
    auto const original = GenerateVertices(30);

    // precompute "expected" verts
    auto const expected = MapToVector(original, [&transform](auto const& p) { return TransformPoint(transform, p); });

    // create mesh with "original" verts
    Mesh m;
    m.setVerts(original);

    // then apply the transform
    m.transformVerts(transform);

    // the mesh's verts should match expectations
    ASSERT_EQ(m.getVerts(), expected);
}

TEST(Mesh, TransformVertsWithTransformCausesTransformedMeshToNotBeEqualToInitialMesh)
{
    Mesh const m;
    Mesh copy{m};

    ASSERT_EQ(m, copy);

    copy.transformVerts(Identity<Transform>());  // noop transform also triggers this (meshes aren't value-comparable)

    ASSERT_NE(m, copy);
}

TEST(Mesh, TransformVertsWithMat4AppliesTransformToVerts)
{
    Mat4 const mat = ToMat4(Transform{
        .scale = Vec3{0.25f},
        .rotation = Quat{Vec3{Deg2Rad(90.0f), 0.0f, 0.0f}},
        .position = {1.0f, 0.25f, 0.125f},
    });

    // generate "original" verts
    auto const original = GenerateVertices(30);

    // precompute "expected" verts
    auto const expected = MapToVector(original, [&mat](auto const& p) { return TransformPoint(mat, p); });

    // create mesh with "original" verts
    Mesh m;
    m.setVerts(original);

    // then apply the transform
    m.transformVerts(mat);

    // the mesh's verts should match expectations
    ASSERT_EQ(m.getVerts(), expected);
}

TEST(Mesh, TransformVertsWithMat4CausesTransformedMeshToNotBeEqualToInitialMesh)
{
    Mesh const m;
    Mesh copy{m};

    ASSERT_EQ(m, copy);

    copy.transformVerts(Identity<Mat4>());  // noop

    ASSERT_NE(m, copy) << "should be non-equal because mesh equality is reference-based (if it becomes value-based, delete this test)";
}

TEST(Mesh, HasNormalsReturnsFalseForNewlyConstructedMesh)
{
    ASSERT_FALSE(Mesh{}.hasNormals());
}

TEST(Mesh, AssigningOnlyNormalsButNoVertsMakesHasNormalsStillReturnFalse)
{
    Mesh m;
    m.setNormals(GenerateNormals(6));
    ASSERT_FALSE(m.hasNormals()) << "shouldn't have any normals, because the caller didn't first assign any vertices";
}

TEST(Mesh, AssigningNormalsAndThenVerticiesMakesNormalsAssignmentFail)
{
    Mesh m;
    m.setNormals(GenerateNormals(9));
    m.setVerts(GenerateVertices(9));
    ASSERT_FALSE(m.hasNormals()) << "shouldn't have any normals, because the caller assigned the vertices _after_ assigning the normals (must be first)";
}

TEST(Mesh, AssigningVerticesAndThenNormalsMakesHasNormalsReturnTrue)
{
    Mesh m;
    m.setVerts(GenerateVertices(6));
    m.setNormals(GenerateNormals(6));
    ASSERT_TRUE(m.hasNormals()) << "this should work: the caller assigned vertices (good) _and then_ normals (also good)";
}

TEST(Mesh, ClearingMeshClearsHasNormals)
{
    Mesh m;
    m.setVerts(GenerateVertices(3));
    m.setNormals(GenerateNormals(3));
    ASSERT_TRUE(m.hasNormals());
    m.clear();
    ASSERT_FALSE(m.hasNormals());
}

TEST(Mesh, HasNormalsReturnsFalseIfOnlyAssigningVerts)
{
    Mesh m;
    m.setVerts(GenerateVertices(3));
    ASSERT_FALSE(m.hasNormals()) << "shouldn't have normals: the caller didn't assign any vertices first";
}

TEST(Mesh, GetNormalsReturnsEmptyOnDefaultConstruction)
{
    Mesh m;
    ASSERT_TRUE(m.getNormals().empty());
}

TEST(Mesh, AssigningOnlyNormalsMakesGetNormalsReturnNothing)
{
    Mesh m;
    m.setNormals(GenerateNormals(3));

    ASSERT_TRUE(m.getNormals().empty()) << "should be empty, because the caller didn't first assign any vertices";
}

TEST(Mesh, AssigningNormalsAfterVerticesBehavesAsExpected)
{
    Mesh m;
    auto const normals = GenerateNormals(3);

    m.setVerts(GenerateVertices(3));
    m.setNormals(normals);

    ASSERT_EQ(m.getNormals(), normals) << "should assign the normals: the caller did what's expected";
}

TEST(Mesh, AssigningFewerNormalsThanVerticesShouldntAssignTheNormals)
{
    Mesh m;
    m.setVerts(GenerateVertices(9));
    m.setNormals(GenerateNormals(6));  // note: less than num verts
    ASSERT_FALSE(m.hasNormals()) << "normals were not assigned: different size from vertices";
}

TEST(Mesh, AssigningMoreNormalsThanVerticesShouldntAssignTheNormals)
{
    Mesh m;
    m.setVerts(GenerateVertices(9));
    m.setNormals(GenerateNormals(12));
    ASSERT_FALSE(m.hasNormals()) << "normals were not assigned: different size from vertices";
}

TEST(Mesh, SuccessfullyAsssigningNormalsChangesMeshEquality)
{
    Mesh m;
    m.setVerts(GenerateVertices(12));

    Mesh copy{m};
    ASSERT_EQ(m, copy);
    copy.setNormals(GenerateNormals(12));
    ASSERT_NE(m, copy);
}

TEST(Mesh, FailingToAssignNormalsDoesNotChangeMeshEquality)
{
    Mesh m;
    m.setVerts(GenerateVertices(12));

    Mesh copy{m};
    ASSERT_EQ(m, copy);
    copy.setNormals(GenerateNormals(9));  // will fail: different size
    ASSERT_EQ(m, copy);
}

TEST(Mesh, TransformNormalsTransormsTheNormals)
{
    auto const transform = [](Vec3 const& n) { return -n; };
    auto const original = GenerateNormals(16);
    auto const expected = MapToVector(original, transform);

    Mesh m;
    m.setVerts(GenerateVertices(16));
    m.setNormals(original);
    ASSERT_EQ(m.getNormals(), original);
    m.transformNormals(transform);
    ASSERT_EQ(m.getNormals(), expected);
}

TEST(Mesh, HasTexCoordsReturnsFalseForDefaultConstructedMesh)
{
    ASSERT_FALSE(Mesh{}.hasTexCoords());
}

TEST(Mesh, AssigningOnlyTexCoordsCausesHasTexCoordsToReturnFalse)
{
    Mesh m;
    m.setTexCoords(GenerateTexCoords(3));
    ASSERT_FALSE(m.hasTexCoords()) << "texture coordinates not assigned: no vertices";
}

TEST(Mesh, AssigningTexCoordsAndThenVerticesCausesHasTexCoordsToReturnFalse)
{
    Mesh m;
    m.setTexCoords(GenerateTexCoords(3));
    m.setVerts(GenerateVertices(3));
    ASSERT_FALSE(m.hasTexCoords()) << "texture coordinates not assigned: assigned in the wrong order";
}

TEST(Mesh, AssigningVerticesAndThenTexCoordsCausesHasTexCoordsToReturnTrue)
{
    Mesh m;
    m.setVerts(GenerateVertices(6));
    m.setTexCoords(GenerateTexCoords(6));
    ASSERT_TRUE(m.hasTexCoords());
}

TEST(Mesh, GetTexCoordsReturnsEmptyOnDefaultConstruction)
{
    Mesh m;
    ASSERT_TRUE(m.getTexCoords().empty());
}

TEST(Mesh, GetTexCoordsReturnsEmptyIfNoVerticesToAssignTheTexCoordsTo)
{
    Mesh m;
    m.setTexCoords(GenerateTexCoords(6));
    ASSERT_TRUE(m.getTexCoords().empty());
}

TEST(Mesh, GetTexCoordsReturnsSetCoordinatesWhenUsedNormally)
{
    Mesh m;
    m.setVerts(GenerateVertices(12));
    auto const coords = GenerateTexCoords(12);
    m.setTexCoords(coords);
    ASSERT_EQ(m.getTexCoords(), coords);
}

TEST(Mesh, SetTexCoordsDoesNotSetCoordsIfGivenLessCoordsThanVerts)
{
    Mesh m;
    m.setVerts(GenerateVertices(12));
    m.setTexCoords(GenerateTexCoords(9));  // note: less
    ASSERT_FALSE(m.hasTexCoords());
    ASSERT_TRUE(m.getTexCoords().empty());
}

TEST(Mesh, SetTexCoordsDoesNotSetCoordsIfGivenMoreCoordsThanVerts)
{
    Mesh m;
    m.setVerts(GenerateVertices(12));
    m.setTexCoords(GenerateTexCoords(15));  // note: more
    ASSERT_FALSE(m.hasTexCoords());
    ASSERT_TRUE(m.getTexCoords().empty());
}

TEST(Mesh, SuccessfulSetCoordsCausesCopiedMeshToBeNotEqualToOriginalMesh)
{
    Mesh m;
    m.setVerts(GenerateVertices(12));
    Mesh copy{m};
    ASSERT_EQ(m, copy);
    copy.setTexCoords(GenerateTexCoords(12));
    ASSERT_NE(m, copy);
}

TEST(Mesh, FailingSetCoordsCausesCopiedMeshToRemainEqualToOrignalMesh)
{
    Mesh m;
    m.setVerts(GenerateVertices(12));
    Mesh copy{m};
    ASSERT_EQ(m, copy);
    copy.setTexCoords(GenerateTexCoords(15));  // note: wrong size
    ASSERT_EQ(m, copy);
}

TEST(Mesh, TransformTexCoordsAppliesTransformToTexCoords)
{
    auto const transform = [](Vec2 const& uv) { return 0.287f * uv; };
    auto const original = GenerateTexCoords(3);
    auto const expected = MapToVector(original, transform);

    Mesh m;
    m.setVerts(GenerateVertices(3));
    m.setTexCoords(original);
    ASSERT_EQ(m.getTexCoords(), original);
    m.transformTexCoords(transform);
    ASSERT_EQ(m.getTexCoords(), expected);
}

TEST(Mesh, GetColorsInitiallyReturnsEmptySpan)
{
    ASSERT_TRUE(Mesh{}.getColors().empty());
}

TEST(Mesh, GetColorsRemainsEmptyIfAssignedWithNoVerts)
{
    Mesh m;
    ASSERT_TRUE(m.getColors().empty());
    m.setColors(GenerateColors(3));
    ASSERT_TRUE(m.getColors().empty()) << "no verticies to assign colors to";
}

TEST(Mesh, GetColorsReturnsSetColorsWhenAssignedToVertices)
{
    Mesh m;
    m.setVerts(GenerateVertices(9));
    auto const colors = GenerateColors(9);
    m.setColors(colors);
    ASSERT_FALSE(m.getColors().empty());
    ASSERT_EQ(m.getColors(), colors);
}

TEST(Mesh, SetColorsAssignmentFailsIfGivenFewerColorsThanVerts)
{
    Mesh m;
    m.setVerts(GenerateVertices(9));
    m.setColors(GenerateColors(6));  // note: less
    ASSERT_TRUE(m.getColors().empty());
}

TEST(Mesh, SetColorsAssignmentFailsIfGivenMoreColorsThanVerts)
{
    Mesh m;
    m.setVerts(GenerateVertices(9));
    m.setColors(GenerateColors(12));  // note: more
    ASSERT_TRUE(m.getColors().empty());
}

TEST(Mesh, GetTangentsInitiallyReturnsEmptySpan)
{
    Mesh m;
    ASSERT_TRUE(m.getTangents().empty());
}

TEST(Mesh, SetTangentsFailsWhenAssigningWithNoVerts)
{
    Mesh m;
    m.setTangents(GenerateTangents(3));
    ASSERT_TRUE(m.getTangents().empty());
}

TEST(Mesh, SetTangentsWorksWhenAssigningToCorrectNumberOfVertices)
{
    Mesh m;
    m.setVerts(GenerateVertices(15));
    auto const tangents = GenerateTangents(15);
    m.setTangents(tangents);
    ASSERT_FALSE(m.getTangents().empty());
    ASSERT_EQ(m.getTangents(), tangents);
}

TEST(Mesh, SetTangentsFailsIfFewerTangentsThanVerts)
{
    Mesh m;
    m.setVerts(GenerateVertices(15));
    m.setTangents(GenerateTangents(12));  // note: fewer
    ASSERT_TRUE(m.getTangents().empty());
}

TEST(Mesh, SetTangentsFailsIfMoreTangentsThanVerts)
{
    Mesh m;
    m.setVerts(GenerateVertices(15));
    m.setTangents(GenerateTangents(18));  // note: more
    ASSERT_TRUE(m.getTangents().empty());
}

TEST(Mesh, GetNumIndicesReturnsZeroOnDefaultConstruction)
{
    Mesh m;
    ASSERT_EQ(m.getIndices().size(), 0);
}

TEST(Mesh, ForEachIndexedVertNotCalledWithEmptyMesh)
{
    size_t ncalls = 0;
    Mesh{}.forEachIndexedVert([&ncalls](auto&&) { ++ncalls; });
    ASSERT_EQ(ncalls, 0);
}

TEST(Mesh, ForEachIndexedVertNotCalledWhenOnlyVertexDataSupplied)
{
    Mesh m;
    m.setVerts({{Vec3{}, Vec3{}, Vec3{}}});
    size_t ncalls = 0;
    m.forEachIndexedVert([&ncalls](auto&&) { ++ncalls; });
    ASSERT_EQ(ncalls, 0);
}

TEST(Mesh, ForEachIndexedVertCalledWhenSuppliedCorrectlyIndexedMesh)
{
    Mesh m;
    m.setVerts({{Vec3{}, Vec3{}, Vec3{}}});
    m.setIndices(std::to_array<uint16_t>({0, 1, 2}));
    size_t ncalls = 0;
    m.forEachIndexedVert([&ncalls](auto&&) { ++ncalls; });
    ASSERT_EQ(ncalls, 3);
}

TEST(Mesh, ForEachIndexedVertCalledEvenWhenMeshIsNonTriangular)
{
    Mesh m;
    m.setTopology(MeshTopology::Lines);
    m.setVerts({{Vec3{}, Vec3{}, Vec3{}, Vec3{}}});
    m.setIndices(std::to_array<uint16_t>({0, 1, 2, 3}));
    size_t ncalls = 0;
    m.forEachIndexedVert([&ncalls](auto&&) { ++ncalls; });
    ASSERT_EQ(ncalls, 4);
}

TEST(Mesh, ForEachIndexedTriangleNotCalledWithEmptyMesh)
{
    size_t ncalls = 0;
    Mesh{}.forEachIndexedTriangle([&ncalls](auto&&) { ++ncalls; });
    ASSERT_EQ(ncalls, 0);
}

TEST(Mesh, ForEachIndexedTriangleNotCalledWhenMeshhasNoIndicies)
{
    Mesh m;
    m.setVerts({{Vec3{}, Vec3{}, Vec3{}}});  // unindexed
    size_t ncalls = 0;
    m.forEachIndexedTriangle([&ncalls](auto&&) { ++ncalls; });
    ASSERT_EQ(ncalls, 0);
}

TEST(Mesh, ForEachIndexedTriangleCalledIfMeshContainsIndexedTriangles)
{
    Mesh m;
    m.setVerts({{Vec3{}, Vec3{}, Vec3{}}});
    m.setIndices(std::to_array<uint16_t>({0, 1, 2}));
    size_t ncalls = 0;
    m.forEachIndexedTriangle([&ncalls](auto&&) { ++ncalls; });
    ASSERT_EQ(ncalls, 1);
}

TEST(Mesh, ForEachIndexedTriangleNotCalledIfMeshContainsInsufficientIndices)
{
    Mesh m;
    m.setVerts({{Vec3{}, Vec3{}, Vec3{}}});
    m.setIndices(std::to_array<uint16_t>({0, 1}));  // too few
    size_t ncalls = 0;
    m.forEachIndexedTriangle([&ncalls](auto&&) { ++ncalls; });
    ASSERT_EQ(ncalls, 0);
}

TEST(Mesh, ForEachIndexedTriangleCalledMultipleTimesForMultipleTriangles)
{
    Mesh m;
    m.setVerts({{Vec3{}, Vec3{}, Vec3{}}});
    m.setIndices(std::to_array<uint16_t>({0, 1, 2, 1, 2, 0}));
    size_t ncalls = 0;
    m.forEachIndexedTriangle([&ncalls](auto&&) { ++ncalls; });
    ASSERT_EQ(ncalls, 2);
}

TEST(Mesh, ForEachIndexedTriangleNotCalledIfMeshTopologyIsLines)
{
    Mesh m;
    m.setTopology(MeshTopology::Lines);
    m.setVerts({{Vec3{}, Vec3{}, Vec3{}}});
    m.setIndices(std::to_array<uint16_t>({0, 1, 2, 1, 2, 0}));
    size_t ncalls = 0;
    m.forEachIndexedTriangle([&ncalls](auto&&) { ++ncalls; });
    ASSERT_EQ(ncalls, 0);
}

TEST(Mesh, GetBoundsReturnsEmptyBoundsOnInitialization)
{
    Mesh m;
    AABB empty{};
    ASSERT_EQ(m.getBounds(), empty);
}

TEST(Mesh, GetBoundsReturnsEmptyForMeshWithUnindexedVerts)
{
    auto const pyramid = std::to_array<Vec3>(
    {
        {-1.0f, -1.0f, 0.0f},  // base: bottom-left
        { 1.0f, -1.0f, 0.0f},  // base: bottom-right
        { 0.0f,  1.0f, 0.0f},  // base: top-middle
        { 0.0f,  0.0f, 1.0f},  // tip
    });

    Mesh m;
    m.setVerts(pyramid);
    AABB empty{};
    ASSERT_EQ(m.getBounds(), empty);
}

TEST(Mesh, GetBooundsReturnsNonemptyForIndexedVerts)
{
    auto const pyramid = std::to_array<Vec3>(
    {
        {-1.0f, -1.0f, 0.0f},  // base: bottom-left
        { 1.0f, -1.0f, 0.0f},  // base: bottom-right
        { 0.0f,  1.0f, 0.0f},  // base: top-middle
    });
    auto const pyramidIndices = std::to_array<uint16_t>({0, 1, 2});

    Mesh m;
    m.setVerts(pyramid);
    m.setIndices(pyramidIndices);
    ASSERT_EQ(m.getBounds(), AABBFromVerts(pyramid));
}

TEST(Mesh, CanBeComparedForEquality)
{
    Mesh m1;
    Mesh m2;

    (void)(m1 == m2);  // just ensure the expression compiles
}

TEST(Mesh, CopiesAreEqual)
{
    Mesh m;
    Mesh copy{m};  // NOLINT(performance-unnecessary-copy-initialization)

    ASSERT_EQ(m, copy);
}

TEST(Mesh, CanBeComparedForNotEquals)
{
    Mesh m1;
    Mesh m2;

    (void)(m1 != m2);  // just ensure the expression compiles
}

TEST(Mesh, CanBeWrittenToOutputStreamForDebugging)
{
    Mesh m;
    std::stringstream ss;

    ss << m;

    ASSERT_FALSE(ss.str().empty());
}

TEST(Mesh, GetSubMeshCountReturnsZeroForDefaultConstructedMesh)
{
    ASSERT_EQ(Mesh{}.getSubMeshCount(), 0);
}

TEST(Mesh, GetSubMeshCountReturnsZeroForMeshWithSomeData)
{
    auto const pyramid = std::to_array<Vec3>(
    {
        {-1.0f, -1.0f, 0.0f},  // base: bottom-left
        { 1.0f, -1.0f, 0.0f},  // base: bottom-right
        { 0.0f,  1.0f, 0.0f},  // base: top-middle
    });
    auto const pyramidIndices = std::to_array<uint16_t>({0, 1, 2});

    Mesh m;
    m.setVerts(pyramid);
    m.setIndices(pyramidIndices);

    ASSERT_EQ(m.getSubMeshCount(), 0);
}

TEST(Mesh, PushSubMeshDescriptorMakesGetMeshSubCountIncrease)
{
    Mesh m;
    ASSERT_EQ(m.getSubMeshCount(), 0);
    m.pushSubMeshDescriptor(SubMeshDescriptor{0, 10, MeshTopology::Triangles});
    ASSERT_EQ(m.getSubMeshCount(), 1);
    m.pushSubMeshDescriptor(SubMeshDescriptor{5, 30, MeshTopology::Lines});
    ASSERT_EQ(m.getSubMeshCount(), 2);
}

TEST(Mesh, PushSubMeshDescriptorMakesGetSubMeshDescriptorReturnPushedDescriptor)
{
    Mesh m;
    SubMeshDescriptor const descriptor{0, 10, MeshTopology::Triangles};

    ASSERT_EQ(m.getSubMeshCount(), 0);
    m.pushSubMeshDescriptor(descriptor);
    ASSERT_EQ(m.getSubMeshDescriptor(0), descriptor);
}

TEST(Mesh, PushSecondDescriptorMakesGetReturnExpectedResults)
{
    Mesh m;
    SubMeshDescriptor const firstDesc{0, 10, MeshTopology::Triangles};
    SubMeshDescriptor const secondDesc{5, 15, MeshTopology::Lines};

    m.pushSubMeshDescriptor(firstDesc);
    m.pushSubMeshDescriptor(secondDesc);

    ASSERT_EQ(m.getSubMeshCount(), 2);
    ASSERT_EQ(m.getSubMeshDescriptor(0), firstDesc);
    ASSERT_EQ(m.getSubMeshDescriptor(1), secondDesc);
}

TEST(Mesh, GetSubMeshDescriptorThrowsOOBExceptionIfOOBAccessed)
{
    Mesh m;

    ASSERT_EQ(m.getSubMeshCount(), 0);
    ASSERT_ANY_THROW({ m.getSubMeshDescriptor(0); });

    m.pushSubMeshDescriptor({0, 10, MeshTopology::Triangles});
    ASSERT_EQ(m.getSubMeshCount(), 1);
    ASSERT_NO_THROW({ m.getSubMeshDescriptor(0); });
    ASSERT_ANY_THROW({ m.getSubMeshDescriptor(1); });
}

TEST(Mesh, ClearSubMeshDescriptorsDoesNothingOnEmptyMesh)
{
    Mesh m;
    ASSERT_NO_THROW({ m.clearSubMeshDescriptors(); });
}

TEST(Mesh, ClearSubMeshDescriptorsClearsAllDescriptors)
{
    Mesh m;
    m.pushSubMeshDescriptor({0, 10, MeshTopology::Triangles});
    m.pushSubMeshDescriptor({5, 15, MeshTopology::Lines});

    ASSERT_EQ(m.getSubMeshCount(), 2);
    ASSERT_NO_THROW({ m.clearSubMeshDescriptors(); });
    ASSERT_EQ(m.getSubMeshCount(), 0);
}

TEST(Mesh, GeneralClearMethodAlsoClearsSubMeshDescriptors)
{
    Mesh m;
    m.pushSubMeshDescriptor({0, 10, MeshTopology::Triangles});
    m.pushSubMeshDescriptor({5, 15, MeshTopology::Lines});

    ASSERT_EQ(m.getSubMeshCount(), 2);
    ASSERT_NO_THROW({ m.clear(); });
    ASSERT_EQ(m.getSubMeshCount(), 0);
}

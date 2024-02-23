#pragma once

#include <oscar/Maths/AABB.h>
#include <oscar/Maths/Angle.h>
#include <oscar/Maths/Eulers.h>
#include <oscar/Maths/Line.h>
#include <oscar/Maths/Mat3.h>
#include <oscar/Maths/Mat4.h>
#include <oscar/Maths/Quat.h>
#include <oscar/Maths/Sphere.h>
#include <oscar/Maths/Transform.h>
#include <oscar/Maths/Triangle.h>
#include <oscar/Maths/Vec.h>
#include <oscar/Maths/Vec2.h>
#include <oscar/Maths/Vec3.h>
#include <oscar/Maths/Vec4.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <concepts>
#include <initializer_list>
#include <optional>
#include <span>
#include <type_traits>

namespace osc { struct Disc; }
namespace osc { struct Plane; }
namespace osc { struct Rect; }
namespace osc { struct Segment; }

// math helpers: generally handy math functions that aren't attached to a particular
//               osc struct
namespace osc
{
    // computes horizontal FoV for a given vertical FoV + aspect ratio
    Radians VerticalToHorizontalFOV(Radians verticalFOV, float aspectRatio);

    // ----- VecX/MatX helpers -----

    // returns the aspect ratio of the vec (effectively: x/y)
    float AspectRatio(Vec2i);

    // returns the aspect ratio of the vec (effectively: x/y)
    float AspectRatio(Vec2);

    // returns the sum of `n` vectors using the "Kahan Summation Algorithm" to reduce errors, returns {0.0f, 0.0f, 0.0f} if provided no inputs
    Vec3 KahanSum(std::span<Vec3 const>);

    // returns the average of `n` vectors using whichever numerically stable average happens to work
    Vec3 NumericallyStableAverage(std::span<Vec3 const>);

    // returns a normal vector of the supplied (pointed to) triangle (i.e. (v[1]-v[0]) x (v[2]-v[0]))
    Vec3 TriangleNormal(Triangle const&);

    // returns a transform matrix that rotates dir1 to point in the same direction as dir2
    Mat4 Dir1ToDir2Xform(Vec3 const& dir1, Vec3 const& dir2);

    // returns euler angles for performing an intrinsic, step-by-step, rotation about X, Y, and then Z
    Eulers ExtractEulerAngleXYZ(Quat const&);

    // returns euler angles for performing an intrinsic, step-by-step, rotation about X, Y, and then Z
    Eulers ExtractEulerAngleXYZ(Mat4 const&);

    // returns an XY NDC point converted from a screen/viewport point
    //
    // - input screen point has origin in top-left, Y goes down
    // - input screen point has range: (0,0) is top-left, (1, 1) is bottom-right
    // - output NDC point has origin in middle, Y goes up
    // - output NDC point has range: (-1, 1) is top-left, (1, -1) is bottom-right
    Vec2 TopleftRelPosToNDCPoint(Vec2 relpos);

    // returns an XY top-left relative point converted from the given NDC point
    //
    // - input NDC point has origin in the middle, Y goes up
    // - input NDC point has range: (-1, -1) for top-left, (1, -1) is bottom-right
    // - output point has origin in the top-left, Y goes down
    // - output point has range: (0, 0) for top-left, (1, 1) for bottom-right
    Vec2 NDCPointToTopLeftRelPos(Vec2 ndcPos);

    // returns an NDC affine point vector (i.e. {x, y, z, 1.0}) converted from a screen/viewport point
    //
    // - input screen point has origin in top-left, Y goes down
    // - input screen point has range: (0,0) is top-left, (1, 1) is bottom-right
    // - output NDC point has origin in middle, Y goes up
    // - output NDC point has range -1 to +1 in each dimension
    // - output assumes Z is "at the front of the cube" (Z = -1.0f)
    // - output will therefore be: {xNDC, yNDC, -1.0f, 1.0f}
    Vec4 TopleftRelPosToNDCCube(Vec2 relpos);

    // "un-project" a screen/viewport point into 3D world-space, assuming a
    // perspective camera
    //
    // - input screen point has origin in top-left, Y goes down
    // - input screen point has range: (0,0) is top-left, (1, 1) is bottom-right
    // - `cameraWorldspaceOrigin` is the location of the camera in world space
    // - `cameraViewMatrix` transforms points from world-space to view-space
    // - `cameraProjMatrix` transforms points from view-space to world-space
    Line PerspectiveUnprojectTopLeftScreenPosToWorldRay(
        Vec2 relpos,
        Vec3 cameraWorldspaceOrigin,
        Mat4 const& cameraViewMatrix,
        Mat4 const& cameraProjMatrix
    );

    // ----- `Rect` helpers -----

    // returns `Min(rect.p1, rect.p2)`: i.e. the smallest X and the smallest Y of the rectangle's points
    Vec2 MinValuePerDimension(Rect const&);

    template<typename T>
    constexpr T Area(Vec<2, T> const& v) requires std::is_arithmetic_v<T>
    {
        return v.x * v.y;
    }

    // returns the area of the rectangle
    float Area(Rect const&);

    // returns the dimensions of the rectangle
    Vec2 Dimensions(Rect const&);

    // returns bottom-left point of the rectangle
    Vec2 BottomLeft(Rect const&);

    // returns the aspect ratio (width/height) of the rectangle
    float AspectRatio(Rect const&);

    // returns the middle point of the rectangle
    Vec2 Midpoint(Rect const&);

    // returns the smallest rectangle that bounds the provided points
    //
    // note: no points --> zero-sized rectangle at the origin
    Rect BoundingRectOf(std::span<Vec2 const>);

    // returns a rectangle that has been expanded along each edge by the given amount
    //
    // (e.g. expand 1.0f adds 1.0f to both the left edge and the right edge)
    Rect Expand(Rect const&, float);
    Rect Expand(Rect const&, Vec2);

    // returns a rectangle where both p1 and p2 are clamped between min and max (inclusive)
    Rect Clamp(Rect const&, Vec2 const& min, Vec2 const& max);

    // returns a rect, created by mapping an Normalized Device Coordinates (NDC) rect
    // (i.e. -1.0 to 1.0) within a screenspace viewport (pixel units, topleft == (0, 0))
    Rect NdcRectToScreenspaceViewportRect(Rect const& ndcRect, Rect const& viewport);


    // ----- `Sphere` helpers -----

    // returns a sphere that bounds the given vertices
    Sphere BoundingSphereOf(std::span<Vec3 const>);

    // returns a sphere that loosely bounds the given AABB
    Sphere ToSphere(AABB const&);

    // returns an xform that maps an origin centered r=1 sphere into an in-scene sphere
    Mat4 FromUnitSphereMat4(Sphere const&);

    // returns an xform that maps a sphere to another sphere
    Mat4 SphereToSphereMat4(Sphere const&, Sphere const&);
    Transform SphereToSphereTransform(Sphere const&, Sphere const&);

    // returns an AABB that contains the sphere
    AABB ToAABB(Sphere const&);


    // ----- `Line` helpers -----

    // returns a line that has been transformed by the supplied transform matrix
    Line TransformLine(Line const&, Mat4 const&);

    // returns a line that has been transformed by the inverse of the supplied transform
    Line InverseTransformLine(Line const&, Transform const&);


    // ----- `Disc` helpers -----

    // returns an xform that maps a disc to another disc
    Mat4 DiscToDiscMat4(Disc const&, Disc const&);


    // ----- `AABB` helpers -----

    // returns an AABB that is "inverted", such that it's minimum is the largest
    // possible value and its maximum is the smallest possible value
    //
    // handy as a "seed" when union-ing many AABBs, because it won't
    AABB InvertedAABB();

    // returns the centerpoint of an AABB
    Vec3 Midpoint(AABB const&);

    // returns the dimensions of an AABB
    Vec3 Dimensions(AABB const&);

    // returns the half-widths of an AABB (effectively, Dimensions(aabb)/2.0)
    Vec3 HalfWidths(AABB const&);

    // returns the volume of the AABB
    float Volume(AABB const&);

    // returns the smallest AABB that spans both of the provided AABBs
    AABB Union(AABB const&, AABB const&);

    // returns true if the AABB has no extents in any dimension
    bool IsAPoint(AABB const&);

    // returns true if the AABB is an extent in any dimension is zero
    bool IsZeroVolume(AABB const&);

    // returns the *index* of the longest dimension of an AABB
    Vec3::size_type LongestDimIndex(AABB const&);

    // returns the length of the longest dimension of an AABB
    float LongestDim(AABB const&);

    // returns the eight corner points of the cuboid representation of the AABB
    std::array<Vec3, 8> ToCubeVerts(AABB const&);

    // returns an AABB that has been transformed by the given matrix
    AABB TransformAABB(AABB const&, Mat4 const&);

    // returns an AABB that has been transformed by the given transform
    AABB TransformAABB(AABB const&, Transform const&);

    // returns an AAB that tightly bounds the provided triangle
    AABB AABBFromTriangle(Triangle const&);

    // returns an AABB that tightly bounds the provided points
    AABB AABBFromVerts(std::span<Vec3 const>);

    // returns an AABB that tightly bounds the provided points (alias that matches BoundingSphereOf, etc.)
    inline AABB BoundingAABBOf(std::span<Vec3 const> vs) { return AABBFromVerts(vs); }

    // returns an AABB that tightly bounds the points indexed by the provided 32-bit indices
    AABB AABBFromIndexedVerts(std::span<Vec3 const> verts, std::span<uint32_t const> indices);

    // returns an AABB that tightly bounds the points indexed by the provided 16-bit indices
    AABB AABBFromIndexedVerts(std::span<Vec3 const> verts, std::span<uint16_t const> indices);

    // (tries to) return a Normalized Device Coordinate (NDC) rectangle, clamped to the NDC clipping
    // bounds ((-1,-1) to (1, 1)), where the rectangle loosely bounds the given worldspace AABB after
    // projecting it into NDC
    std::optional<Rect> AABBToScreenNDCRect(
        AABB const&,
        Mat4 const& viewMat,
        Mat4 const& projMat,
        float znear,
        float zfar
    );


    // ----- `Segment` helpers -----

    // returns a transform matrix that maps a path segment to another path segment
    Mat4 SegmentToSegmentMat4(Segment const&, Segment const&);

    // returns a transform that maps a path segment to another path segment
    Transform SegmentToSegmentTransform(Segment const&, Segment const&);

    // returns a transform that maps a Y-to-Y (bottom-to-top) cylinder to a segment with the given radius
    Transform YToYCylinderToSegmentTransform(Segment const&, float radius);

    // returns a transform that maps a Y-to-Y (bottom-to-top) cone to a segment with the given radius
    Transform YToYConeToSegmentTransform(Segment const&, float radius);


    // ----- `Transform` helpers -----

    // returns a 3x3 transform matrix equivalent to the provided transform (ignores position)
    Mat3 ToMat3(Transform const&);

    // returns a 4x4 transform matrix equivalent to the provided transform
    Mat4 ToMat4(Transform const&);

    // returns a 4x4 transform matrix equivalent to the inverse of the provided transform
    Mat4 ToInverseMat4(Transform const&);

    // returns a 3x3 normal matrix for the provided transform
    Mat3 ToNormalMatrix(Transform const&);

    // returns a 4x4 normal matrix for the provided transform
    Mat4 ToNormalMatrix4(Transform const&);

    // returns a transform that *tries to* perform the equivalent transform as the provided mat4
    //
    // - not all 4x4 matrices can be expressed as an `Transform` (e.g. those containing skews)
    // - uses matrix decomposition to break up the provided matrix
    // - throws if decomposition of the provided matrix is not possible
    Transform ToTransform(Mat4 const&);

    // returns a unit-length vector that is the equivalent of the provided direction vector after applying the transform
    //
    // effectively, apply the Transform but ignore the `position` (translation) component
    Vec3 TransformDirection(Transform const&, Vec3 const&);

    // returns a unit-length vector that is the equivalent of the provided direction vector after applying the inverse of the transform
    //
    // effectively, apply the inverse transform but ignore the `position` (translation) component
    Vec3 InverseTransformDirection(Transform const&, Vec3 const&);

    // returns a vector that is the equivalent of the provided vector after applying the transform
    Vec3 TransformPoint(Transform const&, Vec3 const&);
    Vec3 TransformPoint(Mat4 const&, Vec3 const&);

    // returns a vector that is the equivalent of the provided vector after applying the inverse of the transform
    Vec3 InverseTransformPoint(Transform const&, Vec3 const&);

    // returns the a quaternion equivalent to the given euler angles
    Quat WorldspaceRotation(Eulers const&);

    // applies a world-space rotation to the transform
    void ApplyWorldspaceRotation(
        Transform& applicationTarget,
        Eulers const& eulerAngles,
        Vec3 const& rotationCenter
    );

    // returns XYZ (pitch, yaw, roll) Euler angles for a one-by-one application of an
    // intrinsic rotations.
    //
    // Each rotation is applied one-at-a-time, to the transformed space, so we have:
    //
    // x-y-z (initial)
    // x'-y'-z' (after first rotation)
    // x''-y''-z'' (after second rotation)
    // x'''-y'''-z''' (after third rotation)
    //
    // Assuming we're doing an XYZ rotation, the first rotation rotates x, the second
    // rotation rotates around y', and the third rotation rotates around z''
    //
    // see: https://en.wikipedia.org/wiki/Euler_angles#Conventions_by_intrinsic_rotations
    Eulers ExtractEulerAngleXYZ(Transform const&);

    // returns XYZ (pitch, yaw, roll) Euler angles for an extrinsic rotation
    //
    // in extrinsic rotations, each rotation happens about a *fixed* coordinate system, which
    // is in contrast to intrinsic rotations, which happen in a coordinate system that's attached
    // to a moving body (the thing being rotated)
    //
    // see: https://en.wikipedia.org/wiki/Euler_angles#Conventions_by_extrinsic_rotations
    Eulers ExtractExtrinsicEulerAnglesXYZ(Transform const&);

    // returns the provided transform, but rotated such that the given axis, as expressed
    // in the original transform, will instead point along the new direction
    Transform PointAxisAlong(Transform const&, int axisIndex, Vec3 const& newDirection);

    // returns the provided transform, but rotated such that the given axis, as expressed
    // in the original transform, will instead point towards the given point
    //
    // alternate explanation: "performs the shortest (angular) rotation of the given
    // transform such that the given axis points towards a point in the same space"
    Transform PointAxisTowards(Transform const&, int axisIndex, Vec3 const& location);

    // returns the provided transform, but intrinsically rotated along the given axis by
    // the given number of radians
    Transform RotateAlongAxis(Transform const&, int axisIndex, Radians);
}

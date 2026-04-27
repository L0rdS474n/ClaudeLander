// src/render/faces.cpp -- Pass 6 face shading + cull definitions.
//
// Cull sense, winding, and brightness formula are LOCKED.  See the banner in
// src/render/faces.hpp before editing.  AC-F22 (cull-sense), AC-F18 (winding),
// and AC-F11 (Y-DOWN brightness) are the dedicated bug-class fences.
//
// Boundary: includes core/ headers only.  No raylib, no world/, no entities/,
// no <random>, no <chrono>.  Enforced by render_no_raylib_or_world_includes.

#include "render/faces.hpp"

#include <cstddef>

#include "core/face_types.hpp"
#include "core/matrix3.hpp"
#include "core/vec3.hpp"

namespace render {

namespace {

// Apply the locked Y-DOWN brightness formula and clamp to [0, 1].
//   brightness = clamp(0.5 + 0.5*(-n.y) + 0.1*(-n.x), 0, 1)
//
// AC-F11 (Y-DOWN fence): n = {0,-1,0} -> 1.0
// AC-F12 (clamp at floor): n = {0,+1,0} -> 0.0
// AC-F13 (X-tweak sign): n = {-1,0,0} -> 0.6;  n = {+1,0,0} -> 0.4
constexpr float compute_brightness(Vec3 normal) noexcept {
    const float raw = 0.5f + 0.5f * (-normal.y) + 0.1f * (-normal.x);
    if (raw < 0.0f) return 0.0f;
    if (raw > 1.0f) return 1.0f;
    return raw;
}

}  // namespace

std::optional<VisibleFace> shade_face(
    std::span<const Vec3> vertices_world,
    core::ShipFace        face,
    Vec3                  camera_position) noexcept {
    // Look up the three world-space vertices for this face.  No bounds check:
    // the caller's contract (and AC-F15 for the kShipFaces table) guarantees
    // the indices are in range.
    const Vec3 v0 = vertices_world[face.v0];
    const Vec3 v1 = vertices_world[face.v1];
    const Vec3 v2 = vertices_world[face.v2];

    // Outward normal via the locked winding (D-OutwardNormals).
    //   normal = normalize(cross(v1 - v0, v2 - v0))
    // Pass 6 verified empirically: the 9 bbcelite face index triples in
    // entities::kShipFaces have been winded so this formula yields outward
    // normals (faces 0-5 had v1 and v2 swapped vs raw bbcelite listing;
    // see docs/plans/pass-6-render-faces.md §3 "ship topology fix").
    const Vec3 edge_a = v1 - v0;
    const Vec3 edge_b = v2 - v0;
    const Vec3 normal = normalize(cross(edge_a, edge_b));

    // Face centroid in world space.
    const Vec3 centroid{
        (v0.x + v1.x + v2.x) / 3.0f,
        (v0.y + v1.y + v2.y) / 3.0f,
        (v0.z + v1.z + v2.z) / 3.0f
    };

    // Cull test (D-CullSense): visible iff dot(normal, centroid - camera) < 0.
    // The >= 0 branch covers both back-facing (positive dot) and edge-on
    // (zero dot) cases; AC-F07, AC-F08, AC-F22 fence each variant.
    const Vec3  to_face   = centroid - camera_position;
    const float facing_dp = dot(normal, to_face);
    if (facing_dp >= 0.0f) {
        return std::nullopt;
    }

    // Visible: assemble the result.  face_index is left at 0; callers that
    // need to identify the face populate it post-call (see header doc).
    return VisibleFace{
        /*face_index   =*/ static_cast<std::uint8_t>(0),
        /*normal_world =*/ normal,
        /*brightness   =*/ compute_brightness(normal)
    };
}

void rotate_vertices(
    std::span<const Vec3> body,
    const Mat3&           orientation,
    Vec3                  translation,
    std::span<Vec3>       world_out) noexcept {
    // Caller contract (AC-F05): body.size() == world_out.size().  We iterate
    // up to body.size() and trust the caller.  No bounds check beyond that.
    const std::size_t n = body.size();
    for (std::size_t i = 0; i < n; ++i) {
        // M * v in column-major form: col[0]*v.x + col[1]*v.y + col[2]*v.z.
        // core::multiply(M, v) implements exactly that; using it keeps this
        // module from re-deriving the layout and risking a row/column flip.
        const Vec3 rotated = multiply(orientation, body[i]);
        world_out[i] = rotated + translation;
    }
}

}  // namespace render

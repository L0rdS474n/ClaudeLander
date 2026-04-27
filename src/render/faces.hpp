// src/render/faces.hpp -- Pass 6 face shading + cull (clean-room).
//
// =============================================================================
//  CULL SENSE (D-CullSense, locked) -- READ BEFORE EDITING
// =============================================================================
//
//   A face is VISIBLE iff dot(normal_world, face_centroid - camera_position) < 0
//   A face is CULLED  iff that dot product is >= 0  (>= so edge-on is culled)
//
//   The bug class is "developer flips the sign in the cull test" -- AC-F22 is
//   the loud guard that pins this exactly:
//     normal {0,0,-1} toward camera at {0,0,-5}  -> VISIBLE
//     normal {0,0,+1} away from camera           -> CULLED (nullopt)
//
//   Do NOT compute dot(normal, camera - centroid).  Do NOT swap the operands
//   in the test.  Do NOT change >= to > "to let edge-on faces through".  If a
//   refactor seems to require any of those, re-read this banner and the spec.
//
// =============================================================================
//  WINDING + NORMAL (D-OutwardNormals, locked)
// =============================================================================
//
//   normal = normalize(cross(v1 - v0, v2 - v0))
//
//   The face index list in entities/ship.hpp uses CCW winding viewed from
//   outside the hull (faces 0-5 have v1 and v2 swapped vs raw bbcelite
//   listing — verified empirically against the 9 ship faces, see Pass 6
//   plan §3 "ship topology fix").  This formula yields an outward-pointing
//   normal.  AC-F18 fences that decision against re-ordering.
//
// =============================================================================
//  BRIGHTNESS (D-BrightnessFormula, locked)
// =============================================================================
//
//   brightness = clamp(0.5 + 0.5*(-normal.y) + 0.1*(-normal.x), 0.0, 1.0)
//
//   World space is Y-DOWN.  At a roof-up face (outward normal {0,-1,0})
//   brightness reaches 1.0 (AC-F11, the Y-DOWN fence).  Do NOT flip the sign
//   on normal.y "to make it Y-up" -- that breaks the fence.  See ADR-0006 for
//   the project-wide single-flip-point rule.
//
// =============================================================================
//
// Boundaries:
//   render/ depends on core/ ONLY.  No raylib, no world/, no entities/,
//   no <random>, no <chrono>.  The render_no_raylib_or_world_includes CTest
//   tripwire enforces this; AC-F80 static_assert backs it at compile time.
//
// All public functions are noexcept (AC-F82) and pure -- no clocks, no PRNG,
// no global mutable state.  AC-F04 / AC-F23 verify bit-identical output
// across two independent runs.

#pragma once

#include <cstdint>
#include <optional>
#include <span>

#include "core/face_types.hpp"
#include "core/matrix3.hpp"
#include "core/vec3.hpp"

namespace render {

// ---------------------------------------------------------------------------
// VisibleFace -- result of a successful shade_face call.
// ---------------------------------------------------------------------------
//
// face_index is reserved for callers that want to remember which mesh face
// produced this record (Pass 7 painter ordering).  shade_face does not know
// its own index in any global table, so it leaves the field at 0; the
// caller is expected to populate it post-call when needed.  No AC inspects
// face_index at present (Pass 6).

struct VisibleFace {
    std::uint8_t face_index;
    Vec3         normal_world;
    float        brightness;
};

// ---------------------------------------------------------------------------
// shade_face -- compute a face's outward normal, cull-test it, and (if
// visible) return its brightness under the locked Y-DOWN lighting formula.
// ---------------------------------------------------------------------------
//
//   vertices_world  -- span of world-space vertices the face indexes into;
//                      must contain at least max(face.v0, face.v1, face.v2)+1
//                      entries.  No bounds checking is performed (the caller
//                      owns the contract; entities::kShipFaces always indexes
//                      into entities::kShipVertices, AC-F15 guards the table).
//   face            -- ShipFace with three vertex indices and a base_colour
//                      (base_colour is not used here; reserved for Pass 7).
//   camera_position -- world-space camera position used for the cull test.
//
// Returns std::nullopt for back-facing or edge-on faces (AC-F07, AC-F08,
// AC-F22).  Otherwise returns a VisibleFace with unit-length normal_world
// (AC-F09) and brightness in [0, 1] (AC-F10).

std::optional<VisibleFace> shade_face(
    std::span<const Vec3> vertices_world,
    core::ShipFace        face,
    Vec3                  camera_position) noexcept;

// ---------------------------------------------------------------------------
// rotate_vertices -- transform body-frame vertices into world space.
// ---------------------------------------------------------------------------
//
//   for each i: world_out[i] = orientation * body[i] + translation
//
// Mat3 is column-major (see core/matrix3.hpp): M*v expands to
//   col[0]*v.x + col[1]*v.y + col[2]*v.z
// which is what core::multiply(M, v) implements.
//
// Spans must satisfy body.size() == world_out.size() (AC-F05).  No bounds
// checking is performed; the caller provides matched-size buffers.

void rotate_vertices(
    std::span<const Vec3> body,
    const Mat3&           orientation,
    Vec3                  translation,
    std::span<Vec3>       world_out) noexcept;

}  // namespace render

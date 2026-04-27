// src/entities/ship.hpp -- Pass 4 ship state and per-frame integrator.
//
// Ship is a plain aggregate.  step() applies the locked composition
//   drag -> gravity -> thrust -> position += velocity
// in that order (planner D-StepOrder, AC-K13).  Position uses the
// POST-thrust velocity, not the pre-step velocity; a stale-velocity bug
// would surface as pos.x == 1.0f instead of 0.984375f in AC-K13.
//
// Layer rules:
//   entities/ --> physics/ + core/  (one-way; no raylib, no world/, no render/)
//   step() lives here, not in physics/, because it mutates a Ship.  physics/
//   exposes pure primitives; entities/ composes them against the ship state.
//
// Vertex constants are the 9-vertex body-frame mesh from
// docs/research/pass-4-ship-kinematics-spec.md.  Face index list is DEFERRED
// to Pass 6 (`render/faces`) -- this module ships only the count.
//
// Tripwire: entities_no_forbidden_includes (CMakeLists.txt) greps this
// directory for raylib/world/render.

#pragma once

#include <array>
#include <cstddef>

#include "core/face_types.hpp"
#include "core/matrix3.hpp"
#include "core/vec3.hpp"
#include "physics/kinematics.hpp"

namespace entities {

// ---------------------------------------------------------------------------
// Mesh dimensions (planner D-FaceCount)
// ---------------------------------------------------------------------------
//
// kShipVertexCount = 9 vertices in the body frame.
// kShipFaceCount   = 9 triangle faces.  The face index list (3 indices per
//                    face) is DEFERRED to Pass 6; only the count lives here.

inline constexpr std::size_t kShipVertexCount = 9;
inline constexpr std::size_t kShipFaceCount   = 9;

// ---------------------------------------------------------------------------
// Body-frame vertex constants (planner D-Vertices)
// ---------------------------------------------------------------------------
//
// Source: docs/research/pass-4-ship-kinematics-spec.md, derived from the
// ARM 8.24 fixed-point values divided by 0x01000000 = 16777216.
// AC-S03 verifies each component within kVertexEps = 5e-4f.

extern const std::array<Vec3, kShipVertexCount> kShipVertices;

// ---------------------------------------------------------------------------
// Body-frame face index list (planner D-FaceList, Pass 6)
// ---------------------------------------------------------------------------
//
// Each face is a triangle with three vertex indices into kShipVertices and a
// base_colour palette code from the original Lander spec table.
//
// AC-F14 -- exactly 9 entries, equal to kShipFaceCount.
// AC-F15 -- all vertex indices < kShipVertexCount.
// AC-F16 -- v0, v1, v2 distinct in every face.
// AC-F17 -- recomputed normals are unit length within kFaceEps.
// AC-F18 -- face 0 outward normal: dot(n, face_centroid - mesh_centroid) > 0.
//
// The winding order (v0, v1, v2) is CCW viewed from outside the hull so that
// normalize(cross(v1-v0, v2-v0)) yields an outward-pointing normal (locked
// planner decision D-OutwardNormals).  Do NOT reshuffle these indices --
// AC-F18 is the bug-class fence for that exact mistake.

using ShipFace = core::ShipFace;

// Faces 0-5 have v1 and v2 swapped vs raw bbcelite listing so that
// cross(v1-v0, v2-v0) yields outward normals under our right-handed
// Y-DOWN convention.  All 9 faces verified outward (Python reference
// computation, Pass 6).  AC-F18 is the bug-class fence.
inline constexpr std::array<ShipFace, kShipFaceCount> kShipFaces = {{
    {0, 5, 1, 0x080},
    {1, 5, 2, 0x040},
    {0, 4, 5, 0x040},
    {2, 5, 3, 0x040},
    {3, 5, 4, 0x040},
    {1, 3, 2, 0x088},
    {0, 3, 4, 0x088},
    {0, 1, 3, 0x044},
    {6, 7, 8, 0xC80},
}};

// ---------------------------------------------------------------------------
// Ship aggregate (planner D-InitialState)
// ---------------------------------------------------------------------------
//
// Default-initialised state matches the spawn position used by AC-S01:
//   position    = {0, 5, 0}     -- altitude(0,0) per Pass 2 terrain
//   velocity    = {0, 0, 0}
//   orientation = identity()    -- nose along +x, roof along +y, side along +z

struct Ship {
    Vec3 position    { 0.0f, 5.0f, 0.0f };
    Vec3 velocity    { 0.0f, 0.0f, 0.0f };
    Mat3 orientation { identity() };
};

// ---------------------------------------------------------------------------
// step -- locked per-frame integrator (planner D-StepOrder, AC-K13)
// ---------------------------------------------------------------------------
//
// Composition order:
//   1. velocity = apply_drag(velocity)
//   2. velocity = apply_gravity(velocity)
//   3. velocity = apply_thrust(velocity, orientation, thrust)
//   4. position += velocity                 // POST-thrust velocity
//
// Pure function of (Ship, ThrustLevel); deterministic.  AC-K14 exercises
// 1000-step bit-identical determinism across two independent runs.

void step(Ship& ship, physics::ThrustLevel thrust) noexcept;

}  // namespace entities

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

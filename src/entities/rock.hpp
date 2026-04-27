// src/entities/rock.hpp -- Pass 10 rock entity (state + per-frame integrator).
//
// Rock is a plain aggregate.  step() applies the locked composition
//   gravity -> position += velocity
// in that order (planner D-RockPhysics, AC-R05/R06).  No drag, no thrust,
// no rotation update at this layer (Pass 13 will own spawn + render).
//
// Layer rules:
//   entities/ --> physics/ + core/  (one-way; no raylib, no world/, no render/)
//   step() lives here, not in physics/, because it mutates a Rock.  physics/
//   exposes pure primitives; entities/ composes them against the rock state.
//
// Vertex constants are the 6-vertex axis-aligned octahedron (planner default,
// preserves "6 vertices, 8 faces" without copying any specific bbcelite
// vertex table -- see docs/research/pass-10-rock-particles-spec.md §10).
// Face index list is DEFERRED to Pass 13 -- this module ships only the count.
//
// Y-DOWN convention: positive y = downward.  apply_gravity ADDS to vy, so
// velocity.y grows positive each step (AC-R10 / AC-Rgrav is the bug-class
// fence for that sign).
//
// Tripwire: entities_no_forbidden_includes (CMakeLists.txt) greps this
// directory for raylib/world/render.

#pragma once

#include <array>
#include <cstddef>

#include "core/matrix3.hpp"
#include "core/vec3.hpp"

namespace entities {

// ---------------------------------------------------------------------------
// Mesh dimensions (planner D-RockMesh)
// ---------------------------------------------------------------------------
//
// kRockVertexCount = 6 vertices in the body frame.
// kRockFaceCount   = 8 triangle faces (octahedron).  The face index list is
//                    DEFERRED to Pass 13; only the count lives here.

inline constexpr std::size_t kRockVertexCount = 6;
inline constexpr std::size_t kRockFaceCount   = 8;

// ---------------------------------------------------------------------------
// Body-frame vertex constants (planner D-RockMesh)
// ---------------------------------------------------------------------------
//
// Six axis-aligned ±1 unit-axis extrema forming a regular octahedron:
//   +x, -x, +y, -y, +z, -z
// AC-R02 verifies the count, AC-R04 verifies each component within kRockEps.

extern const std::array<Vec3, kRockVertexCount> kRockVertices;

// ---------------------------------------------------------------------------
// Rock aggregate (planner D-AliveFlag)
// ---------------------------------------------------------------------------
//
// Default-initialised state matches the spawn defaults used by AC-R01:
//   position    = {0, -50, 0}    -- Y-DOWN: well above launchpad ("from the sky")
//   velocity    = {0,   0, 0}
//   orientation = identity()
//   alive       = true

struct Rock {
    Vec3 position    { 0.0f, -50.0f, 0.0f };
    Vec3 velocity    { 0.0f,   0.0f, 0.0f };
    Mat3 orientation { identity() };
    bool alive       { true };
};

// ---------------------------------------------------------------------------
// step -- locked per-frame integrator (planner D-RockPhysics, AC-R05/R06)
// ---------------------------------------------------------------------------
//
// Composition order:
//   1. if (!alive) return;                          -- dead rock = no-op (AC-R08)
//   2. velocity = apply_gravity(velocity)           -- AC-R05
//   3. position += velocity                         -- POST-gravity velocity (AC-R06)
//
// Pure function of (Rock); deterministic.  AC-R07 exercises 1000-step
// bit-identical determinism across two independent runs.

void step(Rock& rock) noexcept;

}  // namespace entities

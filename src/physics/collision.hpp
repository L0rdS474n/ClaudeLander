// src/physics/collision.hpp -- Pass 9 stateless collision classifier.
//
// =============================================================================
//  Y-DOWN PENETRATION SIGN (D-PenetrationRule, locked) -- READ BEFORE EDITING
// =============================================================================
//
//   In Y-DOWN world space, positive y = downward.  A vertex BELOW ground
//   has world_v.y > terrain_y_at_centroid.  A reversed sign would make the
//   ship report Crashed when ABOVE the terrain and Airborne when it has
//   clipped THROUGH it -- exactly backwards.
//
//   AC-Spenet in tests/test_collision.cpp is the loud guard for this bug
//   class.  Do NOT change `>` to `<` "to fix the sign".  See ADR-0006 and
//   docs/ARCHITECTURE.md for the project-wide single-flip-point rule.
//
// =============================================================================
//  WHY terrain_y_at_centroid IS A PARAMETER (boundary rule)
// =============================================================================
//
//   physics/ is forbidden from including world/ headers (CTest tripwire
//   `physics_no_forbidden_includes`).  classify_collision needs the terrain
//   altitude beneath the ship to compute clearance, but it cannot call
//   terrain::altitude directly.  The caller (Pass 13 game loop) computes
//   `terrain_y = terrain::altitude(centroid_x, centroid_z)` and passes it
//   in.  This keeps physics --> core only.
//
//   For unit tests, synthetic terrain_y values can be supplied directly
//   without ever calling terrain::altitude, which is why test_collision.cpp
//   does not include world/terrain.hpp.
//
// =============================================================================
//  Locked design decisions (planner Gate 1)
// =============================================================================
//
//   D-CollisionEnum     -- Airborne / Landing / Crashed.
//   D-LandingThresholds -- kSafeContactHeight=0.05f, kLandingSpeed=0.01f.
//   D-XZAtCentroid      -- clearance computed against the centroid altitude.
//   D-Stateless         -- noexcept, pure function of parameters.
//   D-NoLut             -- std::abs and std::max only, no lookup tables.
//
//   AC-S82: classify_collision MUST be declared noexcept; static_assert in
//   tests/test_collision.cpp fires at compile time if this drops noexcept.
//
// Layer rules:
//   physics/ --> core/  (one-way; no raylib, no world/, no entities/, no
//                        render/, no <random>, no <chrono>)

#pragma once

#include <span>

#include "core/vec3.hpp"

namespace physics {

// ---------------------------------------------------------------------------
// Locked thresholds (planner D-LandingThresholds)
// ---------------------------------------------------------------------------
//
//   kSafeContactHeight -- maximum clearance (terrain_y - lowest_y) at which
//                         a vertex counts as "in contact" with the ground.
//                         Above this, the ship is Airborne.  At or below
//                         (and not penetrating), it is either Landing or
//                         Crashed depending on velocity.  Tiles.
//
//   kLandingSpeed      -- per-axis maximum |velocity| permitted for a safe
//                         Landing.  Any axis exceeding this within the
//                         contact zone is a Crash.  Tiles/frame.
//
//   The boundary semantics is INCLUSIVE on both: clearance == 0 lands,
//   clearance == kSafeContactHeight lands, |v_axis| == kLandingSpeed lands.
//   AC-S10, AC-S11 nail this.
//
//   Pass 14 may retune empirically; for now these are planner defaults.

inline constexpr float kSafeContactHeight = 0.05f;
inline constexpr float kLandingSpeed      = 0.01f;

// ---------------------------------------------------------------------------
// CollisionState (D-CollisionEnum)
// ---------------------------------------------------------------------------

enum class CollisionState {
    Airborne,  // no ground contact
    Landing,   // within contact height AND all axes within kLandingSpeed
    Crashed,   // any vertex penetrates terrain OR contact at excessive speed
};

// ---------------------------------------------------------------------------
// classify_collision -- pure stateless classifier.
// ---------------------------------------------------------------------------
//
// Inputs:
//   vertices_world         -- transformed body vertices in world space.
//                             May be empty: empty input means there are no
//                             points to test for ground contact, so the
//                             ship is treated as Airborne.
//   terrain_y_at_centroid  -- terrain altitude at the ship's centroid x/z,
//                             pre-computed by the caller (see boundary note
//                             at the top of this header).
//   velocity               -- ship linear velocity, per-axis.
//
// Decision tree (Y-DOWN, see AC-Spenet fence):
//   1. lowest_y = max(v.y for v in vertices_world)   -- "lowest" in Y-DOWN
//      means the largest y value.  Empty input => no penetration possible
//      AND no contact possible => Airborne (early return).
//   2. If lowest_y > terrain_y_at_centroid: a vertex is below ground =>
//      Crashed.
//   3. clearance = terrain_y_at_centroid - lowest_y  (>= 0 by step 2).
//   4. If clearance > kSafeContactHeight: the lowest vertex is above the
//      contact zone => Airborne.  (AC-Sclear fences a missing-step-4 bug.)
//   5. Else (in contact zone): if any |velocity.{x,y,z}| > kLandingSpeed
//      => Crashed.  (AC-Sspeed fences a missing-abs() bug.)
//   6. Else => Landing.

CollisionState classify_collision(
    std::span<const Vec3> vertices_world,
    float terrain_y_at_centroid,
    Vec3 velocity) noexcept;

}  // namespace physics

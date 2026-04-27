// src/physics/collision.cpp -- Pass 9 stateless collision classifier (definition).
//
// Pure function on (vertices_world, terrain_y_at_centroid, velocity).  No
// global state, no clocks, no PRNG.  AC-S18 verifies bit-identical results
// over 100 deterministic samples across two independent runs.
//
// Layer rules respected:
//   physics/ --> core/  (one-way; no raylib, no world/, no entities/, no
//                        render/, no <random>, no <chrono>)
//   Tripwire: physics_no_forbidden_includes greps this directory for those
//   forbidden substrings on #include lines.
//
// See src/physics/collision.hpp for the design banners (D-PenetrationRule,
// boundary rule for terrain_y_at_centroid, AC-Spenet/Sclear/Sspeed fences).

#include "physics/collision.hpp"

#include <cmath>   // std::fabs

namespace physics {

namespace {

// Axis-wise absolute-value speed check.  Pulled into a helper so the
// AC-Sspeed fence (velocity uses |v| per axis) is concentrated in one
// readable spot.  D-NoLut: std::fabs only.
constexpr bool any_axis_exceeds_landing_speed(Vec3 v) noexcept {
    return std::fabs(v.x) > kLandingSpeed
        || std::fabs(v.y) > kLandingSpeed
        || std::fabs(v.z) > kLandingSpeed;
}

}  // namespace

CollisionState classify_collision(
    std::span<const Vec3> vertices_world,
    float terrain_y_at_centroid,
    Vec3 velocity) noexcept {
    // Step 1: find the lowest vertex (max y in Y-DOWN).
    //
    // Empty input: no points to test, so the ship cannot be in contact or
    // penetration -- treat as Airborne.  This matches the contract documented
    // in collision.hpp and avoids returning a default-initialised "lowest_y"
    // that would compare against terrain_y in undefined ways.
    if (vertices_world.empty()) {
        return CollisionState::Airborne;
    }

    float lowest_y = vertices_world[0].y;
    // Skip index 0 (already used to seed lowest_y).  AC-S19 (order
    // independence) is satisfied because max-of-set is order-invariant.
    for (std::size_t i = 1; i < vertices_world.size(); ++i) {
        const float y = vertices_world[i].y;
        if (y > lowest_y) {
            lowest_y = y;
        }
    }

    // Step 2: penetration test.  Y-DOWN: lowest_y > terrain_y means the
    // lowest vertex is BELOW ground (penetrating).
    //
    // AC-Spenet fence: do NOT flip this comparison; see the banner in
    // collision.hpp.  AC-S13 and AC-S17 verify single- and multi-vertex
    // penetration both classify as Crashed.
    if (lowest_y > terrain_y_at_centroid) {
        return CollisionState::Crashed;
    }

    // Step 3: clearance is non-negative after step 2 (lowest_y <= terrain_y).
    const float clearance = terrain_y_at_centroid - lowest_y;

    // Step 4: above the contact zone => Airborne, regardless of velocity.
    //
    // AC-Sclear fence: a missing step 4 would fall through to the speed
    // check and return Landing for a ship hanging 100 tiles above ground
    // with zero velocity.  See the banner in collision.hpp.
    if (clearance > kSafeContactHeight) {
        return CollisionState::Airborne;
    }

    // Step 5: in contact zone.  Boundary inclusive: clearance == 0 is
    // Landing/Crashed, never Airborne (AC-S10).  Per-axis |v| check;
    // boundary inclusive: |v| == kLandingSpeed lands (AC-S11).
    //
    // AC-Sspeed fence: must use absolute value per axis.  See banner.
    if (any_axis_exceeds_landing_speed(velocity)) {
        return CollisionState::Crashed;
    }

    // Step 6: in contact zone, all axes within speed threshold => Landing.
    return CollisionState::Landing;
}

}  // namespace physics

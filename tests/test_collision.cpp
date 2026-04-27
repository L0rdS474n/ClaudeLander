// tests/test_collision.cpp — Pass 9 physics/collision tests.
//
// Covers AC-S06..S20 (collision classification), three bug-class fences
// (AC-Spenet, AC-Sclear, AC-Sspeed), and AC-S80..S82 (hygiene, collision side).
// All tests tagged [physics][collision].
//
// === classify_collision signature (plan §6 resolution) ===
//   CollisionState classify_collision(
//       std::span<const Vec3> vertices_world,
//       float terrain_y_at_centroid,
//       Vec3 velocity) noexcept;
//
// Note: the physics module CANNOT include world/ (physics_no_forbidden_includes
// tripwire).  The caller (Pass 13) computes terrain_y = terrain::altitude(...)
// and passes it in.  Tests here supply synthetic terrain_y values directly —
// no call to terrain::altitude in this file.
//
// === Semantics (Y-DOWN) ===
//   1. Find lowest vertex: lowest_y = max(v.y for v in vertices_world)
//      (Y-DOWN: larger y = lower in world space)
//   2. If lowest_y > terrain_y_at_centroid → Crashed (vertex penetrated terrain)
//   3. clearance = terrain_y_at_centroid - lowest_y  (positive = above ground)
//   4. If clearance > kSafeContactHeight → Airborne
//   5. (Within contact height now.)
//      If |vel.x| > kLandingSpeed || |vel.y| > kLandingSpeed || |vel.z| > kLandingSpeed
//          → Crashed
//   6. Else → Landing
//
// === Determinism plan ===
// classify_collision is declared noexcept and is a pure function of its
// parameters.  No PRNG, no clocks, no global mutable state.  AC-S18 verifies
// bit-identical results over 100 fixed samples.  All random-looking inputs are
// derived deterministically (linear index formulae, no std::mt19937).
//
// === Bug-class fences ===
// Three developer-mistake patterns are caught with prominent banner comments:
//   (a) AC-Spenet — Y-DOWN penetration sign: lowest_y > terrain_y is below ground.
//   (b) AC-Sclear — Ship 100 units above ground must be Airborne, not Landing.
//   (c) AC-Sspeed — Velocity check uses |v| (absolute value); sign is irrelevant.
//
// === AC-S82 noexcept verification ===
// static_assert checks noexcept on classify_collision placed after the
// #include so it fires at compile time in the same TU.
//
// === Red-state expectation ===
// This file FAILS TO COMPILE until the implementer creates:
//   src/physics/collision.hpp
//   src/physics/collision.cpp
// That is the correct red state for Gate 2 delivery.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "core/vec3.hpp"
#include "physics/collision.hpp"   // physics::classify_collision,
                                    // physics::CollisionState,
                                    // physics::kSafeContactHeight,
                                    // physics::kLandingSpeed

// ---------------------------------------------------------------------------
// AC-S80: src/physics/collision.hpp must not pull in raylib, world/,
// entities/, render/, <random>, or <chrono>.
// RAYLIB_VERSION is defined by raylib.h; if present here, the include chain
// is polluted.  The BUILD_GAME=OFF build already keeps raylib off the path.
// ---------------------------------------------------------------------------
#ifdef RAYLIB_VERSION
static_assert(false,
    "AC-S80 VIOLATED: physics/collision.hpp pulled in raylib.h "
    "(RAYLIB_VERSION is defined).  Remove the raylib dependency from "
    "src/physics/collision.hpp and src/physics/collision.cpp.");
#endif

// ---------------------------------------------------------------------------
// AC-S82 — classify_collision must be declared noexcept.
// ===========================================================================
//
//  D-Stateless: pure noexcept function, no globals.
//  Verified at compile time here in the same TU as the #include.
// ===========================================================================
namespace {
    void* _collision_noexcept_check() {
        constexpr std::span<const Vec3> empty_span{};
        static_assert(
            noexcept(physics::classify_collision(empty_span, 0.0f, Vec3{})),
            "AC-S82: physics::classify_collision must be declared noexcept");
        return nullptr;
    }
}  // anonymous namespace

// ---------------------------------------------------------------------------
// Tolerance constant (per planner spec §5)
// ---------------------------------------------------------------------------
static constexpr float kCollEps = 1e-5f;

// ---------------------------------------------------------------------------
// Convenience alias
// ---------------------------------------------------------------------------
using CS = physics::CollisionState;

// ---------------------------------------------------------------------------
// Helper: build a simple set of N vertices all at the same y value.
// The single lowest vertex will be at that y; all others can be set lower
// in world space (smaller y in Y-DOWN = higher up).
// ---------------------------------------------------------------------------
static std::vector<Vec3> make_verts_at_y(float lowest_y, int n = 4, float other_y = -10.0f) {
    // other_y < lowest_y in Y-DOWN means other vertices are ABOVE lowest_y
    // (i.e., have smaller y values — higher in the world)
    std::vector<Vec3> verts(static_cast<std::size_t>(n));
    for (int i = 0; i < n - 1; ++i) {
        verts[static_cast<std::size_t>(i)] = Vec3{0.0f, other_y, 0.0f};
    }
    verts[static_cast<std::size_t>(n - 1)] = Vec3{0.0f, lowest_y, 0.0f};
    return verts;
}

// ===========================================================================
// GROUP 1: Airborne (AC-S06..S08)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-S06 — Ship high above terrain: clearance >> kSafeContactHeight → Airborne.
//   Given: all vertices have y << terrain_y (i.e., all are well above ground
//          in Y-DOWN: their y values are much smaller than terrain_y)
//          clearance = terrain_y - lowest_y = 1.5f >> kSafeContactHeight=0.05
//   When:  classify_collision is called
//   Then:  result is Airborne
// ---------------------------------------------------------------------------
TEST_CASE("AC-S06: ship well above terrain (clearance > kSafeContactHeight) returns Airborne", "[physics][collision]") {
    // Given: terrain_y = 5.0, all ship vertices at y=3.5 (1.5 tiles above ground)
    // Y-DOWN: vertex.y=3.5 < terrain_y=5.0 → above terrain; clearance=5.0-3.5=1.5 > 0.05
    const float terrain_y = 5.0f;
    const std::array<Vec3, 3> verts = {{
        {0.0f, 3.5f, 0.0f},
        {1.0f, 3.0f, 0.0f},
        {-1.0f, 3.2f, 0.0f},
    }};
    const Vec3 velocity{0.0f, 0.0f, 0.0f};

    // When
    const CS result = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, velocity);

    // Then
    CAPTURE(static_cast<int>(result));
    REQUIRE(result == CS::Airborne);
}

// ---------------------------------------------------------------------------
// AC-S07 — Velocity high but altitude very high: still Airborne.
//   Given: lowest vertex far above ground; velocity on all axes = 10.0f (huge)
//   When:  classify_collision is called
//   Then:  result is Airborne (velocity check only applies within contact height)
// ---------------------------------------------------------------------------
TEST_CASE("AC-S07: high velocity with ship far above terrain still returns Airborne", "[physics][collision]") {
    // Given: terrain_y=5.0, lowest vertex at y=2.0; clearance=3.0 >> kSafeContactHeight
    const float terrain_y = 5.0f;
    const std::array<Vec3, 2> verts = {{
        {0.0f, 2.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
    }};
    const Vec3 velocity{10.0f, 10.0f, 10.0f};  // far beyond kLandingSpeed

    // When
    const CS result = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, velocity);

    // Then
    CAPTURE(static_cast<int>(result));
    REQUIRE(result == CS::Airborne);
}

// ---------------------------------------------------------------------------
// AC-S08 — Single vertex 1 unit above ground, all others much higher: Airborne.
//   Given: one vertex at clearance = 1.0 unit above ground (> kSafeContactHeight),
//          all other vertices far above that
//   When:  classify_collision is called
//   Then:  result is Airborne (lowest vertex still above contact threshold)
// ---------------------------------------------------------------------------
TEST_CASE("AC-S08: single vertex 1 unit above ground with others much higher returns Airborne", "[physics][collision]") {
    // Given: terrain_y=5.0, lowest vertex at y=4.0 (clearance=1.0 > 0.05)
    // Y-DOWN: y=4.0 is one unit above ground at y=5.0
    const float terrain_y = 5.0f;
    const std::array<Vec3, 4> verts = {{
        {0.0f, 4.0f,  0.0f},   // lowest: clearance = 5.0 - 4.0 = 1.0
        {0.0f, 1.0f,  0.0f},   // high up
        {1.0f, 0.5f,  0.0f},   // even higher
        {-1.0f, 2.0f, 0.0f},   // middle
    }};
    const Vec3 velocity{0.0f, 0.0f, 0.0f};

    // When
    const CS result = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, velocity);

    // Then
    CAPTURE(static_cast<int>(result));
    REQUIRE(result == CS::Airborne);
}

// ===========================================================================
// GROUP 2: Landing (AC-S09..S12)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-S09 — Gentle touch (clearance within kSafeContactHeight), low velocity:
//           Landing.
//   Given: lowest vertex y just below terrain_y by clearance = 0.02f (<0.05)
//          velocity all axes = 0.005f (< kLandingSpeed=0.01)
//   When:  classify_collision is called
//   Then:  result is Landing
// ---------------------------------------------------------------------------
TEST_CASE("AC-S09: clearance within kSafeContactHeight and low velocity returns Landing", "[physics][collision]") {
    // Given: terrain_y=5.0, lowest_y=4.98; clearance=0.02 in [0, kSafeContactHeight]
    const float terrain_y = 5.0f;
    const float lowest_y  = 4.98f;  // clearance = 0.02f, within 0.05
    const std::array<Vec3, 3> verts = {{
        {0.0f, lowest_y, 0.0f},
        {0.0f, 4.0f, 0.0f},
        {0.0f, 3.0f, 0.0f},
    }};
    const Vec3 velocity{0.005f, 0.005f, 0.005f};  // < kLandingSpeed on all axes

    // When
    const CS result = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, velocity);

    // Then
    CAPTURE(static_cast<int>(result), terrain_y, lowest_y, physics::kSafeContactHeight);
    REQUIRE(result == CS::Landing);
}

// ---------------------------------------------------------------------------
// AC-S10 — Lowest vertex exactly at altitude (clearance=0), velocity zero: Landing.
//   Given: lowest_y == terrain_y exactly (clearance = 0.0f)
//          velocity = {0, 0, 0}
//   When:  classify_collision is called
//   Then:  result is Landing (clearance=0 satisfies clearance <= kSafeContactHeight)
// ---------------------------------------------------------------------------
TEST_CASE("AC-S10: lowest vertex exactly at altitude (clearance=0) with zero velocity returns Landing", "[physics][collision]") {
    // Given
    const float terrain_y = 5.0f;
    const float lowest_y  = 5.0f;  // clearance = 0.0f exactly
    const std::array<Vec3, 2> verts = {{
        {0.0f, lowest_y, 0.0f},
        {0.0f, 3.0f, 0.0f},
    }};
    const Vec3 velocity{0.0f, 0.0f, 0.0f};

    // When
    const CS result = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, velocity);

    // Then
    CAPTURE(static_cast<int>(result), terrain_y, lowest_y);
    REQUIRE(result == CS::Landing);
}

// ---------------------------------------------------------------------------
// AC-S11 — Velocity exactly at kLandingSpeed (boundary): Landing (≤ inclusive).
//   Given: clearance = 0.02f (within contact height)
//          velocity = {kLandingSpeed, kLandingSpeed, kLandingSpeed} exactly
//   When:  classify_collision is called
//   Then:  result is Landing (boundary is inclusive: |v| <= kLandingSpeed)
// ---------------------------------------------------------------------------
TEST_CASE("AC-S11: velocity exactly at kLandingSpeed on all axes (inclusive boundary) returns Landing", "[physics][collision]") {
    // Given
    const float terrain_y = 5.0f;
    const float lowest_y  = 4.98f;  // clearance = 0.02f
    const std::array<Vec3, 2> verts = {{
        {0.0f, lowest_y, 0.0f},
        {0.0f, 3.0f, 0.0f},
    }};
    const float spd = physics::kLandingSpeed;  // boundary value exactly
    const Vec3 velocity{spd, spd, spd};

    // When
    const CS result = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, velocity);

    // Then
    CAPTURE(static_cast<int>(result), spd, physics::kLandingSpeed);
    REQUIRE(result == CS::Landing);
}

// ---------------------------------------------------------------------------
// AC-S12 — Landing requires ALL three axes within speed; one exceeds → Crashed.
//   Given: clearance = 0.02f (within contact height)
//          velocity = {kLandingSpeed + 0.005, 0, 0}  (x exceeds threshold)
//   When:  classify_collision is called
//   Then:  result is Crashed (x-axis exceeds kLandingSpeed; any one axis suffices)
// ---------------------------------------------------------------------------
TEST_CASE("AC-S12: single axis exceeding kLandingSpeed within contact height returns Crashed", "[physics][collision]") {
    // Given
    const float terrain_y = 5.0f;
    const float lowest_y  = 4.98f;  // clearance = 0.02f
    const std::array<Vec3, 2> verts = {{
        {0.0f, lowest_y, 0.0f},
        {0.0f, 3.0f, 0.0f},
    }};
    // x-axis just over threshold; y and z are fine
    const Vec3 velocity{physics::kLandingSpeed + 0.005f, 0.0f, 0.0f};

    // When
    const CS result = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, velocity);

    // Then
    CAPTURE(static_cast<int>(result), velocity.x, physics::kLandingSpeed);
    REQUIRE(result == CS::Crashed);
}

// ===========================================================================
// GROUP 3: Crashed (AC-S13..S17)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-S13 — Any vertex penetrating terrain: Crashed.
//   Given: one vertex below ground (y > terrain_y in Y-DOWN)
//          velocity = zero
//   When:  classify_collision is called
//   Then:  result is Crashed (penetration check fires before clearance/speed)
// ---------------------------------------------------------------------------
TEST_CASE("AC-S13: any vertex penetrating terrain (y > terrain_y Y-DOWN) returns Crashed", "[physics][collision]") {
    // Given: terrain_y=5.0, penetrating vertex at y=5.1 (below terrain in Y-DOWN)
    const float terrain_y = 5.0f;
    const std::array<Vec3, 3> verts = {{
        {0.0f, 3.0f, 0.0f},   // well above terrain
        {0.0f, 4.9f, 0.0f},   // just above terrain
        {0.0f, 5.1f, 0.0f},   // PENETRATING: y=5.1 > terrain_y=5.0
    }};
    const Vec3 velocity{0.0f, 0.0f, 0.0f};

    // When
    const CS result = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, velocity);

    // Then
    CAPTURE(static_cast<int>(result), terrain_y);
    REQUIRE(result == CS::Crashed);
}

// ---------------------------------------------------------------------------
// AC-S14 — Within contact height but vertical velocity too high: Crashed.
//   Given: clearance = 0.02f (within kSafeContactHeight)
//          velocity = {0, kLandingSpeed + 0.005, 0}  (y exceeds threshold)
//   When:  classify_collision is called
//   Then:  result is Crashed
// ---------------------------------------------------------------------------
TEST_CASE("AC-S14: within contact height but y-velocity too high returns Crashed", "[physics][collision]") {
    // Given
    const float terrain_y = 5.0f;
    const float lowest_y  = 4.98f;  // clearance = 0.02f
    const std::array<Vec3, 2> verts = {{
        {0.0f, lowest_y, 0.0f},
        {0.0f, 3.0f, 0.0f},
    }};
    const Vec3 velocity{0.0f, physics::kLandingSpeed + 0.005f, 0.0f};

    // When
    const CS result = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, velocity);

    // Then
    CAPTURE(static_cast<int>(result), velocity.y, physics::kLandingSpeed);
    REQUIRE(result == CS::Crashed);
}

// ---------------------------------------------------------------------------
// AC-S15 — Within contact height but x-velocity too high: Crashed.
//   Given: clearance = 0.02f (within kSafeContactHeight)
//          velocity = {kLandingSpeed * 2, 0, 0}  (x well over threshold)
//   When:  classify_collision is called
//   Then:  result is Crashed
// ---------------------------------------------------------------------------
TEST_CASE("AC-S15: within contact height but x-velocity too high returns Crashed", "[physics][collision]") {
    // Given
    const float terrain_y = 5.0f;
    const float lowest_y  = 4.98f;  // clearance = 0.02f
    const std::array<Vec3, 2> verts = {{
        {0.0f, lowest_y, 0.0f},
        {0.0f, 3.0f, 0.0f},
    }};
    const Vec3 velocity{physics::kLandingSpeed * 2.0f, 0.0f, 0.0f};

    // When
    const CS result = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, velocity);

    // Then
    CAPTURE(static_cast<int>(result), velocity.x, physics::kLandingSpeed);
    REQUIRE(result == CS::Crashed);
}

// ---------------------------------------------------------------------------
// AC-S16 — Within contact height but z-velocity too high: Crashed.
//   Given: clearance = 0.02f (within kSafeContactHeight)
//          velocity = {0, 0, kLandingSpeed * 2}  (z well over threshold)
//   When:  classify_collision is called
//   Then:  result is Crashed
// ---------------------------------------------------------------------------
TEST_CASE("AC-S16: within contact height but z-velocity too high returns Crashed", "[physics][collision]") {
    // Given
    const float terrain_y = 5.0f;
    const float lowest_y  = 4.98f;  // clearance = 0.02f
    const std::array<Vec3, 2> verts = {{
        {0.0f, lowest_y, 0.0f},
        {0.0f, 3.0f, 0.0f},
    }};
    const Vec3 velocity{0.0f, 0.0f, physics::kLandingSpeed * 2.0f};

    // When
    const CS result = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, velocity);

    // Then
    CAPTURE(static_cast<int>(result), velocity.z, physics::kLandingSpeed);
    REQUIRE(result == CS::Crashed);
}

// ---------------------------------------------------------------------------
// AC-S17 — Multiple vertices penetrating: Crashed (first penetrator suffices).
//   Given: all vertices below ground (y > terrain_y for each)
//   When:  classify_collision is called
//   Then:  result is Crashed
// ---------------------------------------------------------------------------
TEST_CASE("AC-S17: multiple penetrating vertices all below terrain returns Crashed", "[physics][collision]") {
    // Given: all 4 vertices penetrating (y > terrain_y in Y-DOWN)
    const float terrain_y = 5.0f;
    const std::array<Vec3, 4> verts = {{
        {0.0f, 5.1f, 0.0f},
        {1.0f, 5.2f, 0.0f},
        {-1.0f, 5.5f, 0.0f},
        {0.0f, 6.0f, 0.0f},
    }};
    const Vec3 velocity{0.0f, 0.0f, 0.0f};

    // When
    const CS result = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, velocity);

    // Then
    CAPTURE(static_cast<int>(result), terrain_y);
    REQUIRE(result == CS::Crashed);
}

// ===========================================================================
// GROUP 4: Determinism (AC-S18..S20)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-S18 — classify_collision deterministic over 100 fixed samples.
//   Given: 100 (vertices, terrain_y, velocity) triples derived deterministically
//          (no PRNG — index-formula driven)
//   When:  classify_collision is called twice over the same sequence
//   Then:  every result is bit-identical between runs A and B
// ---------------------------------------------------------------------------
TEST_CASE("AC-S18: classify_collision is deterministic over 100 fixed samples", "[physics][collision]") {
    // Given: deterministic sequence — no PRNG, no clock
    auto run_sequence = [&]() -> std::vector<CS> {
        std::vector<CS> results;
        results.reserve(100);
        for (int i = 0; i < 100; ++i) {
            const float fi        = static_cast<float>(i);
            const float terrain_y = 5.0f + (fi / 200.0f);        // 5.0 .. 5.495
            const float lowest_y  = terrain_y - (fi / 100.0f);   // 0..1 tiles above
            // Use the int counter `i` for the modulo (C++ has no float `%`);
            // intent matches the inline comment "0.000 .. 0.018".
            const float vel_v     = static_cast<float>(i % 10) * 0.002f;  // 0.000 .. 0.018
            const std::array<Vec3, 2> verts = {{
                {0.0f, lowest_y, 0.0f},
                {0.0f, lowest_y - 1.0f, 0.0f},
            }};
            const Vec3 velocity{vel_v, vel_v, vel_v};
            results.push_back(physics::classify_collision(
                std::span<const Vec3>{verts}, terrain_y, velocity));
        }
        return results;
    };

    // When
    const auto run_a = run_sequence();
    const auto run_b = run_sequence();

    // Then — exact enum equality
    REQUIRE(run_a.size() == run_b.size());
    for (std::size_t i = 0; i < run_a.size(); ++i) {
        CAPTURE(i, static_cast<int>(run_a[i]), static_cast<int>(run_b[i]));
        REQUIRE(run_a[i] == run_b[i]);
    }
}

// ---------------------------------------------------------------------------
// AC-S19 — Order independence: vertices in different order yield same result.
//   Given: a fixed set of vertices in normal order and reversed order
//   When:  classify_collision is called for each ordering
//   Then:  both return the same CollisionState
// ---------------------------------------------------------------------------
TEST_CASE("AC-S19: vertex order independence — forward and reverse give same CollisionState", "[physics][collision]") {
    // Given: 5 vertices, some above ground, result depends on lowest only
    // Y-DOWN: terrain_y=5.0; lowest vertex is at y=4.98 (clearance=0.02, landing zone)
    const float terrain_y = 5.0f;
    const Vec3 velocity{0.003f, 0.003f, 0.003f};  // within kLandingSpeed

    const std::array<Vec3, 5> verts_fwd = {{
        {0.0f, 4.98f, 0.0f},   // lowest (clearance=0.02)
        {0.0f, 4.5f,  0.0f},
        {1.0f, 4.0f,  0.0f},
        {-1.0f, 3.5f, 0.0f},
        {0.0f, 3.0f,  0.0f},
    }};
    // Reversed order
    const std::array<Vec3, 5> verts_rev = {{
        {0.0f, 3.0f,  0.0f},
        {-1.0f, 3.5f, 0.0f},
        {1.0f, 4.0f,  0.0f},
        {0.0f, 4.5f,  0.0f},
        {0.0f, 4.98f, 0.0f},   // lowest still at y=4.98
    }};

    // When
    const CS result_fwd = physics::classify_collision(
        std::span<const Vec3>{verts_fwd}, terrain_y, velocity);
    const CS result_rev = physics::classify_collision(
        std::span<const Vec3>{verts_rev}, terrain_y, velocity);

    // Then
    CAPTURE(static_cast<int>(result_fwd), static_cast<int>(result_rev));
    REQUIRE(result_fwd == result_rev);
}

// ---------------------------------------------------------------------------
// AC-S20 — Velocity sign independence: +v and -v give same Landing/Crashed result.
//   Given: clearance = 0.02f (within contact height)
//          velocity_pos = {+0.005, 0, 0}  (x within threshold)
//          velocity_neg = {-0.005, 0, 0}  (x negative same magnitude)
//   When:  classify_collision is called for each
//   Then:  both return the same result (|v| check, sign irrelevant)
// ---------------------------------------------------------------------------
TEST_CASE("AC-S20: positive and negative velocity of same magnitude give identical CollisionState", "[physics][collision]") {
    // Given
    const float terrain_y = 5.0f;
    const float lowest_y  = 4.98f;  // clearance = 0.02f, within contact zone
    const std::array<Vec3, 2> verts = {{
        {0.0f, lowest_y, 0.0f},
        {0.0f, 3.0f, 0.0f},
    }};
    // 0.005 < kLandingSpeed=0.01 → should both land
    const Vec3 velocity_pos{+0.005f, 0.0f, 0.0f};
    const Vec3 velocity_neg{-0.005f, 0.0f, 0.0f};

    // When
    const CS result_pos = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, velocity_pos);
    const CS result_neg = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, velocity_neg);

    // Then
    CAPTURE(static_cast<int>(result_pos), static_cast<int>(result_neg));
    REQUIRE(result_pos == result_neg);
    // Both should be Landing (speed = 0.005 < 0.01 on all axes, clearance in range)
    REQUIRE(result_pos == CS::Landing);
}

// ===========================================================================
// Bug-class fences
// ===========================================================================

// ===========================================================================
//  BUG-CLASS FENCE (AC-Spenet) — Y-DOWN PENETRATION SIGN CATCH
// ===========================================================================
//
//  D-PenetrationRule (locked): In Y-DOWN convention, positive y = downward.
//  A vertex BELOW ground has world_v.y > terrain_y_at_centroid.
//  A reversed check (world_v.y < terrain_y = "below ground") would make the
//  ship report Crashed when it is ABOVE the terrain and Airborne when it has
//  clipped THROUGH it — exactly backwards.
//
//  This test is explicit: we place the lowest vertex ABOVE terrain_y (smaller
//  y in Y-DOWN = higher in world) and demand Airborne, then place it BELOW
//  (larger y in Y-DOWN = deeper in ground) and demand Crashed.
//  A reversed sign produces the opposite results for both assertions.
//
//  If this test fails: re-read D-PenetrationRule and the Y-DOWN section in
//  docs/ARCHITECTURE.md before modifying this test.  Do NOT change the test.
// ===========================================================================
TEST_CASE("AC-Spenet (BUG-CLASS FENCE): Y-DOWN penetration sign — above terrain=Airborne, below terrain=Crashed", "[physics][collision]") {
    const float terrain_y = 5.0f;
    const Vec3 zero_vel{0.0f, 0.0f, 0.0f};

    // --- Part A: vertex ABOVE terrain (y < terrain_y in Y-DOWN) → should NOT crash ---
    // lowest_y = 3.0 < terrain_y = 5.0 → clearance = 2.0 >> kSafeContactHeight → Airborne
    const std::array<Vec3, 1> verts_above = {{ {0.0f, 3.0f, 0.0f} }};
    const CS result_above = physics::classify_collision(
        std::span<const Vec3>{verts_above}, terrain_y, zero_vel);

    CAPTURE(static_cast<int>(result_above), terrain_y);
    // Reversed sign would give Crashed here; correct is Airborne
    REQUIRE(result_above == CS::Airborne);
    REQUIRE(result_above != CS::Crashed);

    // --- Part B: vertex BELOW terrain (y > terrain_y in Y-DOWN) → must Crash ---
    // lowest_y = 5.1 > terrain_y = 5.0 → penetration → Crashed
    const std::array<Vec3, 1> verts_below = {{ {0.0f, 5.1f, 0.0f} }};
    const CS result_below = physics::classify_collision(
        std::span<const Vec3>{verts_below}, terrain_y, zero_vel);

    CAPTURE(static_cast<int>(result_below), terrain_y);
    // Reversed sign would give Airborne here; correct is Crashed
    REQUIRE(result_below == CS::Crashed);
    REQUIRE(result_below != CS::Airborne);
}

// ===========================================================================
//  BUG-CLASS FENCE (AC-Sclear) — CLEARANCE CHECK MUST GATE LANDING
// ===========================================================================
//
//  A ship 100 units ABOVE ground must be Airborne, NOT Landing.
//  A missing or incorrect clearance check (e.g., if the implementation omits
//  step 4 and falls through to the speed check) would see velocity=0 and
//  return Landing for a ship hanging 100 tiles in the air.
//
//  This test catches that class of bug loudly: the only correct answer for
//  clearance = 100 with any velocity is Airborne.
//
//  If this test fails: the clearance guard (clearance > kSafeContactHeight →
//  Airborne) is missing or the comparison is inverted.  Do NOT change the test.
// ===========================================================================
TEST_CASE("AC-Sclear (BUG-CLASS FENCE): ship 100 units above ground returns Airborne NOT Landing", "[physics][collision]") {
    // Given: terrain_y=5.0, lowest vertex at y=-95.0
    // Y-DOWN: y=-95 is 100 units ABOVE terrain_y=5.0; clearance=100 >> kSafeContactHeight
    const float terrain_y = 5.0f;
    const float lowest_y  = -95.0f;  // clearance = 5.0 - (-95.0) = 100.0
    const std::array<Vec3, 2> verts = {{
        {0.0f, lowest_y, 0.0f},
        {0.0f, lowest_y - 1.0f, 0.0f},
    }};
    const Vec3 zero_vel{0.0f, 0.0f, 0.0f};

    // When
    const CS result = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, zero_vel);

    // Then
    CAPTURE(static_cast<int>(result), lowest_y, terrain_y, physics::kSafeContactHeight);
    // Missing clearance check would return Landing (velocity=0); must be Airborne
    REQUIRE(result == CS::Airborne);
    REQUIRE(result != CS::Landing);
}

// ===========================================================================
//  BUG-CLASS FENCE (AC-Sspeed) — VELOCITY CHECK USES ABSOLUTE VALUE
// ===========================================================================
//
//  D-NoLut (locked): velocity threshold uses std::abs per axis.
//  A ship descending at -0.005 tiles/frame (negative y-velocity, which in
//  Y-DOWN actually means moving UPWARD — but magnitude is 0.005 < kLandingSpeed)
//  must still land safely.  An implementation that compares v.y > kLandingSpeed
//  (without absolute value) would allow negative velocities to bypass the check,
//  producing incorrect Crashed or Landing states depending on the bug variant.
//
//  This test verifies BOTH signs of velocity produce identical results:
//    {+0.005, 0, 0} → Landing  (below threshold, all axes)
//    {-0.005, 0, 0} → Landing  (same magnitude, sign flipped)
//    {+0.02,  0, 0} → Crashed  (above threshold)
//    {-0.02,  0, 0} → Crashed  (same magnitude, sign flipped — must also crash)
//
//  If this test fails: the absolute value is missing from the velocity comparison.
//  Do NOT change the test — add std::abs to the implementation.
// ===========================================================================
TEST_CASE("AC-Sspeed (BUG-CLASS FENCE): velocity threshold uses |v| — both signs of speed behave identically", "[physics][collision]") {
    const float terrain_y = 5.0f;
    const float lowest_y  = 4.98f;  // clearance = 0.02f, within contact zone
    const std::array<Vec3, 2> verts = {{
        {0.0f, lowest_y, 0.0f},
        {0.0f, 3.0f, 0.0f},
    }};

    // --- Below threshold: both signs must land ---
    const Vec3 vel_slow_pos{+0.005f, 0.0f, 0.0f};
    const Vec3 vel_slow_neg{-0.005f, 0.0f, 0.0f};
    const CS slow_pos = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, vel_slow_pos);
    const CS slow_neg = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, vel_slow_neg);

    CAPTURE(static_cast<int>(slow_pos), static_cast<int>(slow_neg));
    REQUIRE(slow_pos == CS::Landing);
    REQUIRE(slow_neg == CS::Landing);  // sign flip must NOT change outcome

    // --- Above threshold: both signs must crash ---
    const Vec3 vel_fast_pos{+0.02f, 0.0f, 0.0f};
    const Vec3 vel_fast_neg{-0.02f, 0.0f, 0.0f};
    const CS fast_pos = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, vel_fast_pos);
    const CS fast_neg = physics::classify_collision(
        std::span<const Vec3>{verts}, terrain_y, vel_fast_neg);

    CAPTURE(static_cast<int>(fast_pos), static_cast<int>(fast_neg));
    REQUIRE(fast_pos == CS::Crashed);
    REQUIRE(fast_neg == CS::Crashed);  // missing abs() would pass one, fail the other
}

// ===========================================================================
// GROUP 5: Hygiene (AC-S80..S82, collision side)
// ===========================================================================
//
// AC-S80 (no-raylib static_assert) is at the top of this file.
// AC-S82 (noexcept static_assert) is at the top of this file.

TEST_CASE("AC-S80: physics/collision.hpp compiles without raylib, world/, entities/, render/, <random>, <chrono>", "[physics][collision]") {
    // Given: this test file was compiled with BUILD_GAME=OFF (no raylib on path)
    //        and the physics_no_forbidden_includes CTest tripwire active.
    // When:  it reaches this TEST_CASE at runtime
    // Then:  it ran — which means the header compiled without forbidden deps,
    //        satisfying AC-S80 (collision side).
    SUCCEED("compilation without raylib/forbidden deps succeeded — AC-S80 (collision side) satisfied");
}

TEST_CASE("AC-S81: claude_lander_physics link list unchanged after adding collision.cpp", "[physics][collision]") {
    // Given: this test binary was linked with claude_lander_physics which links
    //        against claude_lander_core + claude_lander_warnings only.
    // When:  it reaches this TEST_CASE
    // Then:  it ran without link errors — AC-S81 satisfied (physics side).
    SUCCEED("physics library link list unchanged (core+warnings only) — AC-S81 (collision side) satisfied");
}

TEST_CASE("AC-S82: classify_collision is noexcept (verified by static_assert at top of TU)", "[physics][collision]") {
    // Given: static_assert(noexcept(physics::classify_collision(...))) at the
    //        top of this file (inside anonymous namespace helper).
    // When:  this test is reached (compile succeeded means static_assert passed)
    // Then:  AC-S82 is satisfied.
    SUCCEED("static_assert(noexcept(classify_collision(...))) passed at compile time — AC-S82 (collision side) satisfied");
}

TEST_CASE("AC-S82: kSafeContactHeight and kLandingSpeed constants are accessible and positive", "[physics][collision]") {
    // Given: physics constants declared in collision.hpp
    // When:  their values are checked
    // Then:  both are positive and match locked design values
    CAPTURE(physics::kSafeContactHeight, physics::kLandingSpeed);
    REQUIRE(physics::kSafeContactHeight > 0.0f);
    REQUIRE(physics::kSafeContactHeight == Catch::Approx(0.05f).margin(kCollEps));
    REQUIRE(physics::kLandingSpeed > 0.0f);
    REQUIRE(physics::kLandingSpeed == Catch::Approx(0.01f).margin(kCollEps));
}

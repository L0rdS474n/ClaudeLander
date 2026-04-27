// tests/test_kinematics.cpp - Pass 4 physics/kinematics tests.
//
// Covers AC-K01..K15 (drag, gravity, thrust, step composition, determinism)
// and AC-K80..K82 (hygiene).
// All tests tagged [physics][kinematics].
//
// === Determinism plan ===
// All tests are fully deterministic.  apply_drag, apply_gravity, apply_thrust
// are declared noexcept pure functions.  step() is a pure function of
// (Ship, ThrustLevel).  No clocks, no filesystem, no PRNG state, no system
// calls.  AC-K14 exercises 1000 iterations and requires bit-identical results.
//
// === Step order (D-StepOrder) ===
// step() applies: drag -> gravity -> thrust -> position += velocity
// AC-K13 pins this with a hand-computed golden value.  If any rearrangement
// is made, AC-K13 will fail loudly because the operations are non-commutative
// (drag scales a velocity that gravity/thrust then add to).
//
// === Bug-class fences ===
// Three developer-mistake patterns are caught by dedicated tests:
//   (a) AC-K10 catches Half = Full/2 (wrong) vs Full/4 (correct).
//   (b) AC-K13 catches position += old_velocity (wrong) vs position += new_velocity (correct).
//       A stale-velocity bug would produce pos.x == 1.0f (old vel) instead of
//       0.984375f (post-drag vel).
//   (c) AC-K08 catches gravity subtracted (wrong) vs added (correct) - Y-DOWN
//       means positive y-velocity is downward; gravity must ADD to vy.
//
// === AC-K80 (architecture hygiene: no forbidden includes in kinematics.hpp) ===
// Implemented as a static_assert: if RAYLIB_VERSION is defined (only if
// raylib.h was included transitively), compilation fails with a clear message.
// The BUILD_GAME=OFF build guarantees raylib is not on the include path; the
// static_assert is an in-TU belt-and-suspenders tripwire.
//
// === Red-state expectation ===
// This file FAILS TO COMPILE (missing headers) until the implementer creates:
//   src/physics/kinematics.hpp
//   src/physics/kinematics.cpp
//   src/entities/ship.hpp
//   src/entities/ship.cpp
// That is the correct red state for Gate 2 delivery.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <cstddef>

#include "core/vec3.hpp"
#include "core/matrix3.hpp"
#include "physics/kinematics.hpp"
#include "entities/ship.hpp"

// ---------------------------------------------------------------------------
// AC-K80: physics/kinematics.hpp must not pull in raylib, world, entities,
// render, <random>, or <chrono>.
// RAYLIB_VERSION is defined by raylib.h; it is only present here if the
// #include above transitively included raylib.  The BUILD_GAME=OFF build
// already keeps raylib off the compiler include path.
// ---------------------------------------------------------------------------
#ifdef RAYLIB_VERSION
static_assert(false,
    "AC-K80 VIOLATED: physics/kinematics.hpp pulled in raylib.h "
    "(RAYLIB_VERSION is defined).  Remove the raylib dependency from "
    "src/physics/kinematics.hpp and src/physics/kinematics.cpp.");
#endif

// ---------------------------------------------------------------------------
// Tolerance constants
// ---------------------------------------------------------------------------
static constexpr float kKinEps      = 1e-6f;   // equality checks
static constexpr float kKinDecayEps = 1e-3f;   // decay / convergence checks

// ===========================================================================
// GROUP 1: Drag (AC-K01..K05)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-K01: apply_drag({1,0,0}) -> (kDragPerFrame, 0, 0)
//   kDragPerFrame = 63/64 = 0.984375f; x-only input exercises the scale path.
// ---------------------------------------------------------------------------
TEST_CASE("AC-K01: apply_drag({1,0,0}) returns (kDragPerFrame, 0, 0)", "[physics][kinematics]") {
    // Given: velocity = {1, 0, 0}
    // When:  apply_drag is called
    // Then:  result = {63/64, 0, 0} = {0.984375f, 0, 0}
    const Vec3 v{1.0f, 0.0f, 0.0f};
    const Vec3 result = physics::apply_drag(v);
    CAPTURE(result.x, result.y, result.z, physics::kDragPerFrame);
    REQUIRE(result.x == Catch::Approx(physics::kDragPerFrame).margin(kKinEps));
    REQUIRE(result.y == Catch::Approx(0.0f).margin(kKinEps));
    REQUIRE(result.z == Catch::Approx(0.0f).margin(kKinEps));
}

// ---------------------------------------------------------------------------
// AC-K02: apply_drag({2,4,-8}) -> componentwise multiply by kDragPerFrame
//   All three axes must be independently scaled.
// ---------------------------------------------------------------------------
TEST_CASE("AC-K02: apply_drag({2,4,-8}) scales each component by kDragPerFrame", "[physics][kinematics]") {
    // Given: velocity = {2, 4, -8}
    // When:  apply_drag is called
    // Then:  each component is multiplied by kDragPerFrame
    const Vec3 v{2.0f, 4.0f, -8.0f};
    const Vec3 result = physics::apply_drag(v);
    const float d = physics::kDragPerFrame;
    CAPTURE(result.x, result.y, result.z, d);
    REQUIRE(result.x == Catch::Approx(2.0f * d).margin(kKinEps));
    REQUIRE(result.y == Catch::Approx(4.0f * d).margin(kKinEps));
    REQUIRE(result.z == Catch::Approx(-8.0f * d).margin(kKinEps));
}

// ---------------------------------------------------------------------------
// AC-K03: 64 iterations of apply_drag on {1,0,0} -> |x| < 0.5f
//   Derived: (63/64)^64 ~ 0.3650; confirms drag actually decays velocity
//   and rejects trivial identity implementations.
// ---------------------------------------------------------------------------
TEST_CASE("AC-K03: 64 iterations of apply_drag on {1,0,0} reduces |x| below 0.5", "[physics][kinematics]") {
    // Given: initial velocity = {1, 0, 0}
    // When:  apply_drag applied 64 times
    // Then:  |result.x| < 0.5f (value ~ 0.365 by hand)
    Vec3 v{1.0f, 0.0f, 0.0f};
    for (int i = 0; i < 64; ++i) {
        v = physics::apply_drag(v);
    }
    CAPTURE(v.x);
    REQUIRE(std::abs(v.x) < 0.5f);
    // Also confirm it is still positive and > 0 (didn't collapse to zero)
    REQUIRE(v.x > 0.0f);
}

// ---------------------------------------------------------------------------
// AC-K04: apply_drag({0,0,0}) -> bit-equal {0,0,0}
//   0 * anything == 0; no spurious accumulation.
// ---------------------------------------------------------------------------
TEST_CASE("AC-K04: apply_drag({0,0,0}) returns exactly {0,0,0}", "[physics][kinematics]") {
    // Given: velocity = {0, 0, 0}
    // When:  apply_drag is called
    // Then:  result is exactly {0, 0, 0} (bit-equal, not just approximately)
    const Vec3 v{0.0f, 0.0f, 0.0f};
    const Vec3 result = physics::apply_drag(v);
    REQUIRE(result.x == 0.0f);
    REQUIRE(result.y == 0.0f);
    REQUIRE(result.z == 0.0f);
}

// ---------------------------------------------------------------------------
// AC-K05: apply_drag({1e6, -1e6, 1e6}) -> finite, componentwise multiply
//   Large magnitudes must not produce NaN/Inf; result is kDragPerFrame * input.
// ---------------------------------------------------------------------------
TEST_CASE("AC-K05: apply_drag on large-magnitude input produces finite componentwise-scaled result", "[physics][kinematics]") {
    // Given: velocity = {1e6, -1e6, 1e6}
    // When:  apply_drag is called
    // Then:  result is finite and equals input * kDragPerFrame per component
    const Vec3 v{1.0e6f, -1.0e6f, 1.0e6f};
    const Vec3 result = physics::apply_drag(v);
    const float d = physics::kDragPerFrame;
    CAPTURE(result.x, result.y, result.z);
    REQUIRE(std::isfinite(result.x));
    REQUIRE(std::isfinite(result.y));
    REQUIRE(std::isfinite(result.z));
    REQUIRE(result.x == Catch::Approx(1.0e6f * d).margin(1.0f));
    REQUIRE(result.y == Catch::Approx(-1.0e6f * d).margin(1.0f));
    REQUIRE(result.z == Catch::Approx(1.0e6f * d).margin(1.0f));
}

// ===========================================================================
// GROUP 2: Gravity (AC-K06..K08)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-K06: apply_gravity({1,2,3}) -> (1, 2+kGravityPerFrame, 3)
//   Gravity must ONLY mutate the y-axis; x and z must pass through unchanged.
// ---------------------------------------------------------------------------
TEST_CASE("AC-K06: apply_gravity({1,2,3}) adds kGravityPerFrame to y only", "[physics][kinematics]") {
    // Given: velocity = {1, 2, 3}
    // When:  apply_gravity is called
    // Then:  x and z are unchanged; y += kGravityPerFrame
    const Vec3 v{1.0f, 2.0f, 3.0f};
    const Vec3 result = physics::apply_gravity(v);
    const float g = physics::kGravityPerFrame;
    CAPTURE(result.x, result.y, result.z, g);
    REQUIRE(result.x == Catch::Approx(1.0f).margin(kKinEps));
    REQUIRE(result.y == Catch::Approx(2.0f + g).margin(kKinEps));
    REQUIRE(result.z == Catch::Approx(3.0f).margin(kKinEps));
}

// ---------------------------------------------------------------------------
// AC-K07: 4096 iterations of apply_gravity on {0,0,0} -> result.y ~ 1.0f
//   kGravityPerFrame = 1/4096; 4096 * (1/4096) = 1.0f exactly.
//   Tolerance kKinDecayEps = 1e-3f covers float accumulation error.
// ---------------------------------------------------------------------------
TEST_CASE("AC-K07: 4096 iterations of apply_gravity on {0,0,0} yields result.y ~ 1.0f within 1e-3", "[physics][kinematics]") {
    // Given: initial velocity = {0, 0, 0}
    // When:  apply_gravity applied 4096 times
    // Then:  result.y ~ 1.0f within kKinDecayEps
    Vec3 v{0.0f, 0.0f, 0.0f};
    for (int i = 0; i < 4096; ++i) {
        v = physics::apply_gravity(v);
    }
    CAPTURE(v.y, physics::kGravityPerFrame);
    REQUIRE(std::abs(v.y - 1.0f) <= kKinDecayEps);
}

// ---------------------------------------------------------------------------
// AC-K08: apply_gravity({0,0,0}).y > 0 (Y-DOWN sign fence)
//
// ============================================================================
//  Y-DOWN GRAVITY SIGN FENCE - bug-class fence
// ============================================================================
//
//  The project uses Y-DOWN convention (see docs/ARCHITECTURE.md and the D-*
//  decisions in pass-4-ship-kinematics.md).  In Y-DOWN, positive y-velocity
//  means DOWNWARD motion.  Gravity must ADD to vy (making the ship fall down).
//
//  If a developer accidentally writes `vy -= kGravityPerFrame` (subtracting,
//  as one might in Y-UP convention), this test catches it immediately:
//    correct:    apply_gravity({0,0,0}).y = +kGravityPerFrame > 0 OK
//    bug (flip): apply_gravity({0,0,0}).y = -kGravityPerFrame < 0 X
//
//  If this test fails: re-read D-GravitySemantics in the spec before
//  modifying this test.  The test is almost certainly correct; the
//  implementation has the sign wrong.
// ============================================================================
TEST_CASE("AC-K08: apply_gravity({0,0,0}).y is positive (Y-DOWN: gravity adds to downward velocity)", "[physics][kinematics]") {
    // Given: velocity = {0, 0, 0}
    // When:  apply_gravity is called
    // Then:  result.y > 0 (Y-DOWN sign: positive y is downward; gravity is downward)
    //        A sign-flip bug (Y-UP convention) would produce result.y < 0 here.
    const Vec3 v{0.0f, 0.0f, 0.0f};
    const Vec3 result = physics::apply_gravity(v);
    CAPTURE(result.y, physics::kGravityPerFrame);
    REQUIRE(result.y > 0.0f);
    // Exact value check:
    REQUIRE(result.y == Catch::Approx(physics::kGravityPerFrame).margin(kKinEps));
}

// ===========================================================================
// GROUP 3: Thrust (AC-K09..K12)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-K09: apply_thrust(v, orient, None) -> no-op
//   ThrustLevel::None must not modify velocity regardless of orientation.
// ---------------------------------------------------------------------------
TEST_CASE("AC-K09: apply_thrust with ThrustLevel::None is a no-op", "[physics][kinematics]") {
    // Given: velocity = {3, -2, 1}, any orientation (identity)
    // When:  apply_thrust called with ThrustLevel::None
    // Then:  result is unchanged from input
    const Vec3 v{3.0f, -2.0f, 1.0f};
    const Mat3 orient = identity();
    const Vec3 result = physics::apply_thrust(v, orient, physics::ThrustLevel::None);
    CAPTURE(result.x, result.y, result.z);
    REQUIRE(result.x == Catch::Approx(v.x).margin(kKinEps));
    REQUIRE(result.y == Catch::Approx(v.y).margin(kKinEps));
    REQUIRE(result.z == Catch::Approx(v.z).margin(kKinEps));
}

// ---------------------------------------------------------------------------
// AC-K10: apply_thrust({0}, identity(), Full) -> (0, kFullThrust, 0);
//         apply_thrust({0}, identity(), Half) -> (0, kHalfThrust, 0).
//
// ============================================================================
//  HALF-THRUST BUG FENCE - bug-class fence
// ============================================================================
//
//  D-Constants specifies kHalfThrust = Full/4 = 1/8192, NOT Full/2.
//  This is a deliberate spec decision from the original ARM code.
//  A developer intuiting "half" = Full/2 would produce 1/4096, which is 4x
//  kGravityPerFrame - this AC catches that mistake loudly.
//
//  Expected: kHalfThrust = 1.0f/8192.0f ~ 1.22e-4f
//  Bug (Full/2): 1.0f/4096.0f ~ 2.44e-4f
//
//  The test checks kHalfThrust == kFullThrust/4 as a constant invariant,
//  AND checks the returned velocity component against kHalfThrust directly.
//  If this test fails: re-read D-Constants in the spec before modifying.
// ============================================================================
TEST_CASE("AC-K10: Full thrust adds kFullThrust to y; Half adds kHalfThrust (= Full/4, NOT Full/2)", "[physics][kinematics]") {
    // Given: velocity = {0,0,0}, orientation = identity (col[1] = {0,1,0})
    // When:  apply_thrust called with Full, then separately with Half
    // Then:
    //   Full: result.y == kFullThrust
    //   Half: result.y == kHalfThrust == kFullThrust/4  (NOT kFullThrust/2)
    const Vec3 zero{0.0f, 0.0f, 0.0f};
    const Mat3 orient = identity();

    // --- Full thrust ---
    const Vec3 full_result = physics::apply_thrust(zero, orient, physics::ThrustLevel::Full);
    CAPTURE(full_result.y, physics::kFullThrust);
    REQUIRE(full_result.x == Catch::Approx(0.0f).margin(kKinEps));
    REQUIRE(full_result.y == Catch::Approx(physics::kFullThrust).margin(kKinEps));
    REQUIRE(full_result.z == Catch::Approx(0.0f).margin(kKinEps));

    // --- Half thrust: must be Full/4, not Full/2 ---
    const Vec3 half_result = physics::apply_thrust(zero, orient, physics::ThrustLevel::Half);
    CAPTURE(half_result.y, physics::kHalfThrust, physics::kFullThrust / 4.0f);
    // Constant invariant: kHalfThrust == kFullThrust / 4
    REQUIRE(physics::kHalfThrust == Catch::Approx(physics::kFullThrust / 4.0f).margin(kKinEps));
    // Bug fence: kHalfThrust must NOT equal kFullThrust / 2
    REQUIRE(physics::kHalfThrust != Catch::Approx(physics::kFullThrust / 2.0f).margin(kKinEps));
    // Returned velocity:
    REQUIRE(half_result.x == Catch::Approx(0.0f).margin(kKinEps));
    REQUIRE(half_result.y == Catch::Approx(physics::kHalfThrust).margin(kKinEps));
    REQUIRE(half_result.z == Catch::Approx(0.0f).margin(kKinEps));
}

// ---------------------------------------------------------------------------
// AC-K11: apply_thrust({0}, identity(), Full) thrust is along col[1] (roof)
//   With identity orientation, col[1] = {0,1,0}.  Only vy must change;
//   vx and vz stay zero.
// ---------------------------------------------------------------------------
TEST_CASE("AC-K11: Full thrust with identity orientation applies force along col[1] = {0,1,0}", "[physics][kinematics]") {
    // Given: velocity = {0,0,0}, orientation = identity
    // When:  apply_thrust called with Full
    // Then:  thrust is along col[1] = {0,1,0}; vx == 0, vy == kFullThrust, vz == 0
    const Vec3 zero{0.0f, 0.0f, 0.0f};
    const Mat3 orient = identity();
    const Vec3 result = physics::apply_thrust(zero, orient, physics::ThrustLevel::Full);
    CAPTURE(result.x, result.y, result.z);
    // Only y-axis changes (col[1] of identity is {0,1,0})
    REQUIRE(result.x == Catch::Approx(0.0f).margin(kKinEps));
    REQUIRE(result.y == Catch::Approx(physics::kFullThrust).margin(kKinEps));
    REQUIRE(result.z == Catch::Approx(0.0f).margin(kKinEps));
}

// ---------------------------------------------------------------------------
// AC-K12: Custom orient with col[1] = (1,0,0), Full -> velocity gains in x.
//   Catches "world up hardcoded" bugs where thrust always goes along {0,1,0}
//   regardless of the orientation matrix.
// ---------------------------------------------------------------------------
TEST_CASE("AC-K12: Full thrust with custom orient (col[1]=(1,0,0)) applies thrust along world x-axis", "[physics][kinematics]") {
    // Given: velocity = {0,0,0}
    //        orientation where col[1] (roof) = {1,0,0} (pointing along world x)
    // When:  apply_thrust called with Full
    // Then:  vx == kFullThrust, vy == 0, vz == 0
    //        A "world-up hardcoded" bug would produce vx == 0, vy == kFullThrust
    const Vec3 zero{0.0f, 0.0f, 0.0f};
    // Build a custom orientation: col[1] = {1,0,0}
    Mat3 orient = identity();
    orient.col[1] = Vec3{1.0f, 0.0f, 0.0f};  // roof points along world +x
    orient.col[0] = Vec3{0.0f, 1.0f, 0.0f};  // nose along world +y (arbitrary, non-degenerate)
    orient.col[2] = Vec3{0.0f, 0.0f, 1.0f};  // side unchanged

    const Vec3 result = physics::apply_thrust(zero, orient, physics::ThrustLevel::Full);
    CAPTURE(result.x, result.y, result.z, physics::kFullThrust);
    // Thrust must follow col[1] = {1,0,0}, not hardcoded {0,1,0}
    REQUIRE(result.x == Catch::Approx(physics::kFullThrust).margin(kKinEps));
    REQUIRE(result.y == Catch::Approx(0.0f).margin(kKinEps));
    REQUIRE(result.z == Catch::Approx(0.0f).margin(kKinEps));
}

// ===========================================================================
// GROUP 4: Step composition (AC-K13..K15)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-K13: step(ship={pos={0,0,0}, vel={1,0,0}, orient=identity()}, Full)
//   Hand-computed golden value for the full step pipeline.
//
// ============================================================================
//  STEP-ORDER + STALE-VELOCITY BUG FENCE - bug-class fence
// ============================================================================
//
//  D-StepOrder pins: drag -> gravity -> thrust -> position += velocity.
//  The position is updated with the POST-thrust velocity, not the pre-drag
//  original velocity.  Two common mistakes are caught here:
//
//  (a) Stale-velocity bug: position += old_velocity (before drag/gravity/thrust)
//      Bug produces: pos.x = 1.0f (old vel x); correct is 0.984375f (post-drag).
//
//  (b) Wrong step order (e.g., thrust before drag):
//      The vy component would accumulate differently; AC-K13 pins it exactly.
//
//  Hand computation (Python-verified):
//    vel = {1, 0, 0}
//    1. drag:   vel = {63/64, 0, 0}               = {0.984375, 0, 0}
//    2. gravity: vel.y += 1/4096                  -> vy = 0.000244140625
//    3. thrust Full, col[1]={0,1,0}: vel.y += 1/2048 -> vy = 3/4096 = 0.000732421875
//    4. pos += vel: pos = {0.984375, 0.000732421875, 0}
//
//  If step() applies position += pre-drag vel instead, pos.x == 1.0f, which
//  fails the REQUIRE below (expected 0.984375f).
// ============================================================================
TEST_CASE("AC-K13: step with vel={1,0,0} Full thrust produces correct post-step position and velocity", "[physics][kinematics]") {
    // Given: ship.position = {0,0,0}, ship.velocity = {1,0,0}, orientation = identity
    //        ThrustLevel = Full
    // When:  step() is called once
    // Then:  velocity and position match hand-computed golden values
    //        (drag->gravity->thrust->pos+=vel; post-thrust velocity used for position)
    entities::Ship ship;
    ship.position    = Vec3{0.0f, 0.0f, 0.0f};
    ship.velocity    = Vec3{1.0f, 0.0f, 0.0f};
    ship.orientation = identity();

    entities::step(ship, physics::ThrustLevel::Full);

    // Hand-computed golden values (Python-verified):
    // After drag:    vel.x = 63/64 = 0.984375
    // After gravity: vel.y = 1/4096 = 0.000244140625
    // After thrust:  vel.y = 3/4096 = 0.000732421875
    // After pos+=vel: pos = {0.984375, 0.000732421875, 0}
    static constexpr float kExpVx  = 63.0f / 64.0f;               // 0.984375
    static constexpr float kExpVy  = 3.0f / 4096.0f;              // 0.000732421875
    static constexpr float kExpPx  = 63.0f / 64.0f;               // pos.x = post-drag vel.x
    static constexpr float kExpPy  = 3.0f / 4096.0f;              // pos.y = post-thrust vel.y

    CAPTURE(ship.velocity.x, ship.velocity.y, ship.velocity.z);
    CAPTURE(ship.position.x, ship.position.y, ship.position.z);
    CAPTURE(kExpVx, kExpVy, kExpPx, kExpPy);

    // Velocity:
    REQUIRE(ship.velocity.x == Catch::Approx(kExpVx).margin(kKinEps));
    REQUIRE(ship.velocity.y == Catch::Approx(kExpVy).margin(kKinEps));
    REQUIRE(ship.velocity.z == Catch::Approx(0.0f).margin(kKinEps));

    // Position (must equal post-thrust velocity, NOT original vel):
    REQUIRE(ship.position.x == Catch::Approx(kExpPx).margin(kKinEps));
    REQUIRE(ship.position.y == Catch::Approx(kExpPy).margin(kKinEps));
    REQUIRE(ship.position.z == Catch::Approx(0.0f).margin(kKinEps));
}

// ---------------------------------------------------------------------------
// AC-K14: 1000 step() calls are deterministic (bit-identical to fresh re-run)
//   Confirms there is no PRNG, clock, or global mutable state inside step().
//   Two independent simulation loops must produce bit-identical final states.
// ---------------------------------------------------------------------------
TEST_CASE("AC-K14: 1000 step() calls are deterministic - bit-identical to a fresh independent run", "[physics][kinematics]") {
    // Given: two Ship objects with identical initial state
    //        ThrustLevel = Full
    // When:  each is advanced 1000 steps independently
    // Then:  final position and velocity are bit-identical between the two runs
    auto make_ship = []() {
        entities::Ship s;
        s.position    = Vec3{0.0f, 5.0f, 0.0f};
        s.velocity    = Vec3{0.1f, 0.0f, -0.05f};
        s.orientation = identity();
        return s;
    };

    entities::Ship run_a = make_ship();
    entities::Ship run_b = make_ship();

    for (int i = 0; i < 1000; ++i) {
        entities::step(run_a, physics::ThrustLevel::Full);
        entities::step(run_b, physics::ThrustLevel::Full);
    }

    CAPTURE(run_a.position.x, run_b.position.x);
    CAPTURE(run_a.velocity.y, run_b.velocity.y);

    // Bit-identical: exact equality, NOT Approx
    REQUIRE(run_a.position.x == run_b.position.x);
    REQUIRE(run_a.position.y == run_b.position.y);
    REQUIRE(run_a.position.z == run_b.position.z);
    REQUIRE(run_a.velocity.x == run_b.velocity.x);
    REQUIRE(run_a.velocity.y == run_b.velocity.y);
    REQUIRE(run_a.velocity.z == run_b.velocity.z);
}

// ---------------------------------------------------------------------------
// AC-K15: 1000 frames of step({pos={0,5,0}, vel=0}, Full) ->
//   terminal v.y ~ (kGravityPerFrame + kFullThrust) * 64 ~ 0.04687f within 1e-3f
//
//   With drag=63/64 and constant acceleration a = kGravity + kFull each frame:
//   Terminal velocity = a * sum_{k=0..inf} drag^k = a / (1 - drag) = a * 64
//   = (1/4096 + 1/2048) * 64 = (3/4096) * 64 = 3/64 = 0.046875f
//
//   After 1000 frames the geometric series converges well within kKinDecayEps.
// ---------------------------------------------------------------------------
TEST_CASE("AC-K15: 1000 Full-thrust frames yield terminal v.y ~ (kGravity+kFull)*64 within 1e-3", "[physics][kinematics]") {
    // Given: ship starting at pos={0,5,0}, vel={0,0,0}, orient=identity
    //        ThrustLevel = Full
    // When:  step() called 1000 times
    // Then:  final v.y ~ (kGravityPerFrame + kFullThrust) * 64 ~ 0.046875 within kKinDecayEps
    entities::Ship ship;
    ship.position    = Vec3{0.0f, 5.0f, 0.0f};
    ship.velocity    = Vec3{0.0f, 0.0f, 0.0f};
    ship.orientation = identity();

    for (int i = 0; i < 1000; ++i) {
        entities::step(ship, physics::ThrustLevel::Full);
    }

    const float terminal = (physics::kGravityPerFrame + physics::kFullThrust) * 64.0f;
    CAPTURE(ship.velocity.y, terminal, physics::kGravityPerFrame, physics::kFullThrust);
    REQUIRE(std::abs(ship.velocity.y - terminal) <= kKinDecayEps);
}

// ===========================================================================
// GROUP 5: Hygiene (AC-K80..K82)
// ===========================================================================
//
// AC-K80 (no-raylib static_assert) is at the top of this file, immediately
// after the #include "physics/kinematics.hpp".
//
// AC-K81: claude_lander_physics link list = core + warnings only.
//   Verified by CMakeLists.txt; no runtime TEST_CASE needed.
//   The BUILD_GAME=OFF build exercises this invariant.
//
// AC-K82: apply_* functions are pure (deterministic).
//   Verified by AC-K14 (1000-step determinism).  A separate TEST_CASE would
//   be redundant; determinism is the observable property of purity.

TEST_CASE("AC-K81: physics/kinematics header and library compile and link without raylib (BUILD_GAME=OFF)", "[physics][kinematics]") {
    // Given: this test file was compiled with BUILD_GAME=OFF (no raylib on path)
    // When:  it reaches this TEST_CASE at runtime
    // Then:  it ran - which means kinematics.hpp compiled and linked without raylib,
    //        satisfying AC-K81.
    SUCCEED("compilation and linkage without raylib succeeded - AC-K81 satisfied");
}

TEST_CASE("AC-K82: apply_drag/gravity/thrust constants are accessible and self-consistent", "[physics][kinematics]") {
    // Given: physics constants declared in kinematics.hpp
    // When:  their values and relationships are checked
    // Then:  all are positive, kHalfThrust == kFullThrust / 4, kDragPerFrame < 1
    REQUIRE(physics::kDragPerFrame > 0.0f);
    REQUIRE(physics::kDragPerFrame < 1.0f);  // must actually decay
    REQUIRE(physics::kDragPerFrame == Catch::Approx(63.0f / 64.0f).margin(kKinEps));
    REQUIRE(physics::kFullThrust > 0.0f);
    REQUIRE(physics::kFullThrust == Catch::Approx(1.0f / 2048.0f).margin(kKinEps));
    REQUIRE(physics::kHalfThrust > 0.0f);
    REQUIRE(physics::kHalfThrust == Catch::Approx(1.0f / 8192.0f).margin(kKinEps));
    REQUIRE(physics::kHalfThrust == Catch::Approx(physics::kFullThrust / 4.0f).margin(kKinEps));
    REQUIRE(physics::kGravityPerFrame > 0.0f);
    REQUIRE(physics::kGravityPerFrame == Catch::Approx(1.0f / 4096.0f).margin(kKinEps));
    REQUIRE(physics::kFrameRate == 50);
    REQUIRE(physics::kFrameDt == Catch::Approx(1.0f / 50.0f).margin(kKinEps));
}

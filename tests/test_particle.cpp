// tests/test_particle.cpp -- Pass 10 entities/particle tests.
//
// Covers AC-R11..R20 (Particle step semantics, TTL, gravity, kind dispatch)
// and hygiene AC-R80..R83.  Bug-class fences AC-Pttl and AC-Pexh are inline
// within their respective TEST_CASEs.
// All tests tagged [entities][particle].
//
// === Determinism plan ===
// All tests are fully deterministic.  step(Particle&) is a pure function of
// its input state: no clocks, no PRNG, no global mutable state, no I/O.
// AC-R18 exercises 1000 iterations and requires bit-identical results from
// two independent runs.
//
// === Kind dispatch ===
// Only Exhaust is exempt from gravity.  Explosion, Bullet, Spark all receive
// gravity.  Tests AC-R14..R17 each exercise a distinct kind by checking
// whether velocity.y changes.
//
// === Bug-class fences ===
// AC-Pttl  (AC-R19): step on a dead particle (ttl==0) must be a complete
//   no-op; position, velocity, and ttl are all unchanged.
// AC-Pexh  (AC-R20): exhaust velocity must be preserved across a step
//   (only position changes; gravity is NOT applied).  A missing exhaust-guard
//   would change velocity.y, failing the exact equality check below.
//
// === AC-R80 architecture hygiene ===
// RAYLIB_VERSION is defined only if raylib.h was transitively included.
// The static_assert below catches that at compile time.
//
// === Red-state expectation ===
// This file FAILS TO COMPILE (missing headers) until the implementer creates:
//   src/entities/particle.hpp
//   src/entities/particle.cpp
// That is the correct red state for Gate 2 delivery.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "core/vec3.hpp"
#include "core/matrix3.hpp"
#include "physics/kinematics.hpp"
#include "entities/particle.hpp"

// ---------------------------------------------------------------------------
// AC-R80: entities/particle.hpp must not pull in raylib, world, render, or
// platform headers.  RAYLIB_VERSION is defined by raylib.h; only present here
// if the #include above transitively included raylib.
// ---------------------------------------------------------------------------
#ifdef RAYLIB_VERSION
static_assert(false,
    "AC-R80 VIOLATED: entities/particle.hpp pulled in raylib.h "
    "(RAYLIB_VERSION is defined).  Remove the raylib dependency from "
    "src/entities/particle.hpp and src/entities/particle.cpp.");
#endif

// ---------------------------------------------------------------------------
// Tolerance constant
// ---------------------------------------------------------------------------
static constexpr float kParticleEps = 1e-6f;

// ===========================================================================
// GROUP 1: Dead particle (ttl == 0) is a no-op (AC-R11 / AC-Pttl)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R11: Default ttl=0 -> step is a no-op.
//   A default-constructed Particle has ttl=0 by the spec; step must leave
//   everything unchanged.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R11: Particle with default ttl=0 - step is a no-op (position, velocity, ttl unchanged)", "[entities][particle]") {
    // Given: a Particle with ttl=0 (default / dead)
    // When:  step() called
    // Then:  position unchanged, velocity unchanged, ttl still 0
    entities::Particle p{};
    p.position = Vec3{1.0f, 2.0f, 3.0f};
    p.velocity = Vec3{0.1f, 0.2f, 0.3f};
    p.ttl      = 0;
    p.kind     = entities::ParticleKind::Explosion;

    entities::step(p);

    CAPTURE(p.position.x, p.position.y, p.position.z);
    CAPTURE(p.velocity.x, p.velocity.y, p.velocity.z);
    CAPTURE(p.ttl);
    REQUIRE(p.position.x == Catch::Approx(1.0f).margin(kParticleEps));
    REQUIRE(p.position.y == Catch::Approx(2.0f).margin(kParticleEps));
    REQUIRE(p.position.z == Catch::Approx(3.0f).margin(kParticleEps));
    REQUIRE(p.velocity.x == Catch::Approx(0.1f).margin(kParticleEps));
    REQUIRE(p.velocity.y == Catch::Approx(0.2f).margin(kParticleEps));
    REQUIRE(p.velocity.z == Catch::Approx(0.3f).margin(kParticleEps));
    REQUIRE(p.ttl == 0);
}

// ===========================================================================
// GROUP 2: TTL decrement (AC-R12)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R12: ttl=10 -> step decrements ttl to 9.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R12: Particle with ttl=10 - step decrements ttl to 9", "[entities][particle]") {
    // Given: Particle with ttl=10, kind=Explosion, velocity=zero
    // When:  step() called once
    // Then:  ttl == 9
    entities::Particle p{};
    p.position = Vec3{0.0f, 0.0f, 0.0f};
    p.velocity = Vec3{0.0f, 0.0f, 0.0f};
    p.ttl      = 10;
    p.kind     = entities::ParticleKind::Explosion;

    entities::step(p);

    CAPTURE(p.ttl);
    REQUIRE(p.ttl == 9);
}

// ===========================================================================
// GROUP 3: TTL goes to zero after last step (AC-R13)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R13: ttl=1 -> step -> ttl=0; subsequent step is a no-op.
//   After the first step ttl reaches zero; the second step must not
//   change position or velocity.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R13: Particle with ttl=1 - step decrements to 0; next step is no-op", "[entities][particle]") {
    // Given: Particle with ttl=1, non-zero velocity, kind=Bullet
    // When:  step() called once (ttl -> 0), then again (no-op)
    // Then:
    //   After first step: ttl == 0, position changed by velocity, velocity
    //     changed by gravity.
    //   After second step: position and velocity and ttl unchanged (dead).
    entities::Particle p{};
    p.position = Vec3{0.0f, 0.0f, 0.0f};
    p.velocity = Vec3{1.0f, 0.0f, 0.0f};
    p.ttl      = 1;
    p.kind     = entities::ParticleKind::Bullet;

    entities::step(p);

    CAPTURE(p.ttl);
    REQUIRE(p.ttl == 0);

    // Capture state after first step; second step must leave these unchanged.
    const Vec3 pos_after_first = p.position;
    const Vec3 vel_after_first = p.velocity;

    entities::step(p);

    CAPTURE(p.position.x, p.position.y, p.position.z);
    CAPTURE(p.velocity.x, p.velocity.y, p.velocity.z);
    REQUIRE(p.position.x == pos_after_first.x);
    REQUIRE(p.position.y == pos_after_first.y);
    REQUIRE(p.position.z == pos_after_first.z);
    REQUIRE(p.velocity.x == vel_after_first.x);
    REQUIRE(p.velocity.y == vel_after_first.y);
    REQUIRE(p.velocity.z == vel_after_first.z);
    REQUIRE(p.ttl == 0);
}

// ===========================================================================
// GROUP 4: Kind dispatch - gravity (AC-R14..R17)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R14: Exhaust kind: gravity NOT applied to velocity.
//   After one step on an Exhaust particle with zero velocity, velocity.y
//   must still be 0 (gravity was not added).
// ---------------------------------------------------------------------------
TEST_CASE("AC-R14: Exhaust particle - gravity is NOT applied to velocity across a step", "[entities][particle]") {
    // Given: Exhaust particle, zero velocity, ttl=5
    // When:  step() called once
    // Then:  velocity.y == 0 (gravity not applied; Exhaust exception to gravity)
    entities::Particle p{};
    p.position = Vec3{0.0f, 0.0f, 0.0f};
    p.velocity = Vec3{0.0f, 0.0f, 0.0f};
    p.ttl      = 5;
    p.kind     = entities::ParticleKind::Exhaust;

    entities::step(p);

    CAPTURE(p.velocity.x, p.velocity.y, p.velocity.z, physics::kGravityPerFrame);
    REQUIRE(p.velocity.x == Catch::Approx(0.0f).margin(kParticleEps));
    REQUIRE(p.velocity.y == Catch::Approx(0.0f).margin(kParticleEps));
    REQUIRE(p.velocity.z == Catch::Approx(0.0f).margin(kParticleEps));
}

// ---------------------------------------------------------------------------
// AC-R15: Explosion kind: gravity applied to velocity.
//   After one step with zero velocity, velocity.y must equal kGravityPerFrame.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R15: Explosion particle - gravity IS applied to velocity across a step", "[entities][particle]") {
    // Given: Explosion particle, zero velocity, ttl=5
    // When:  step() called once
    // Then:  velocity.y == kGravityPerFrame (gravity was added)
    entities::Particle p{};
    p.position = Vec3{0.0f, 0.0f, 0.0f};
    p.velocity = Vec3{0.0f, 0.0f, 0.0f};
    p.ttl      = 5;
    p.kind     = entities::ParticleKind::Explosion;

    entities::step(p);

    const float g = physics::kGravityPerFrame;
    CAPTURE(p.velocity.x, p.velocity.y, p.velocity.z, g);
    REQUIRE(p.velocity.y == Catch::Approx(g).margin(kParticleEps));
}

// ---------------------------------------------------------------------------
// AC-R16: Bullet kind: gravity applied to velocity.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R16: Bullet particle - gravity IS applied to velocity across a step", "[entities][particle]") {
    // Given: Bullet particle, zero velocity, ttl=5
    // When:  step() called once
    // Then:  velocity.y == kGravityPerFrame
    entities::Particle p{};
    p.position = Vec3{0.0f, 0.0f, 0.0f};
    p.velocity = Vec3{0.0f, 0.0f, 0.0f};
    p.ttl      = 5;
    p.kind     = entities::ParticleKind::Bullet;

    entities::step(p);

    const float g = physics::kGravityPerFrame;
    CAPTURE(p.velocity.x, p.velocity.y, p.velocity.z, g);
    REQUIRE(p.velocity.y == Catch::Approx(g).margin(kParticleEps));
}

// ---------------------------------------------------------------------------
// AC-R17: Spark kind: gravity applied to velocity.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R17: Spark particle - gravity IS applied to velocity across a step", "[entities][particle]") {
    // Given: Spark particle, zero velocity, ttl=5
    // When:  step() called once
    // Then:  velocity.y == kGravityPerFrame
    entities::Particle p{};
    p.position = Vec3{0.0f, 0.0f, 0.0f};
    p.velocity = Vec3{0.0f, 0.0f, 0.0f};
    p.ttl      = 5;
    p.kind     = entities::ParticleKind::Spark;

    entities::step(p);

    const float g = physics::kGravityPerFrame;
    CAPTURE(p.velocity.x, p.velocity.y, p.velocity.z, g);
    REQUIRE(p.velocity.y == Catch::Approx(g).margin(kParticleEps));
}

// ===========================================================================
// GROUP 5: Determinism (AC-R18)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R18: 1000 steps are bit-identical across two independent runs.
//   Uses an Explosion particle (gravity applies) with non-zero initial state
//   to exercise the full step path including gravity accumulation.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R18: 1000 step(particle) calls are deterministic - bit-identical to a fresh independent run", "[entities][particle]") {
    // Given: two Particle objects with identical initial state, kind=Explosion
    // When:  each is advanced 1000 steps independently
    // Then:  final position, velocity, and ttl are bit-identical

    auto make_particle = []() {
        entities::Particle p{};
        p.position = Vec3{1.0f, 0.0f, -2.0f};
        p.velocity = Vec3{0.05f, 0.0f, -0.05f};
        p.ttl      = 60000;  // large enough to survive 1000 steps
        p.kind     = entities::ParticleKind::Explosion;
        p.color    = 0x0080;
        return p;
    };

    entities::Particle run_a = make_particle();
    entities::Particle run_b = make_particle();

    for (int i = 0; i < 1000; ++i) {
        entities::step(run_a);
        entities::step(run_b);
    }

    CAPTURE(run_a.position.x, run_b.position.x);
    CAPTURE(run_a.velocity.y, run_b.velocity.y);
    CAPTURE(run_a.ttl,        run_b.ttl);

    // Bit-identical: exact equality, NOT Approx
    REQUIRE(run_a.position.x == run_b.position.x);
    REQUIRE(run_a.position.y == run_b.position.y);
    REQUIRE(run_a.position.z == run_b.position.z);
    REQUIRE(run_a.velocity.x == run_b.velocity.x);
    REQUIRE(run_a.velocity.y == run_b.velocity.y);
    REQUIRE(run_a.velocity.z == run_b.velocity.z);
    REQUIRE(run_a.ttl == run_b.ttl);
}

// ===========================================================================
// GROUP 6: Bug-class fence - dead particle is complete no-op (AC-R19 / AC-Pttl)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R19 (AC-Pttl): step on dead particle (ttl==0) does absolutely nothing.
//
// ============================================================================
//  DEAD-PARTICLE NO-OP FENCE - bug-class fence
// ============================================================================
//
//  A common mistake is to decrement ttl before the early-exit guard, reaching
//  underflow (0 -> 65535 for uint16_t) and letting the particle "revive".
//  This test catches that by using ttl=0 with a non-zero velocity and
//  confirming ttl stays 0 and nothing changes.
//
//  Correct:   if (p.ttl == 0) return; // then decrement at end
//  Bug (underflow): --p.ttl; if (p.ttl == 65535u) ... // silent wrap
// ============================================================================
TEST_CASE("AC-R19 (AC-Pttl): step on dead particle (ttl=0) is a complete no-op - no underflow, no mutation", "[entities][particle]") {
    // Given: Explosion particle with ttl=0 (dead) and non-zero velocity
    // When:  step() called
    // Then:  position unchanged, velocity unchanged, ttl still 0 (no underflow)
    entities::Particle p{};
    p.position = Vec3{5.0f, -3.0f, 2.0f};
    p.velocity = Vec3{1.0f,  1.0f, 1.0f};
    p.ttl      = 0;
    p.kind     = entities::ParticleKind::Explosion;

    entities::step(p);

    CAPTURE(p.position.x, p.position.y, p.position.z);
    CAPTURE(p.velocity.x, p.velocity.y, p.velocity.z);
    CAPTURE(p.ttl);
    REQUIRE(p.position.x == Catch::Approx(5.0f).margin(kParticleEps));
    REQUIRE(p.position.y == Catch::Approx(-3.0f).margin(kParticleEps));
    REQUIRE(p.position.z == Catch::Approx(2.0f).margin(kParticleEps));
    REQUIRE(p.velocity.x == Catch::Approx(1.0f).margin(kParticleEps));
    REQUIRE(p.velocity.y == Catch::Approx(1.0f).margin(kParticleEps));
    REQUIRE(p.velocity.z == Catch::Approx(1.0f).margin(kParticleEps));
    // ttl must not underflow (uint16_t 0 -> 65535 would be the bug)
    REQUIRE(p.ttl == static_cast<std::uint16_t>(0));
}

// ===========================================================================
// GROUP 7: Bug-class fence - exhaust velocity preserved (AC-R20 / AC-Pexh)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R20 (AC-Pexh): exhaust velocity preserved across step (only position
//         changes from the existing velocity; gravity is NOT added).
//
// ============================================================================
//  EXHAUST-GRAVITY-BYPASS FENCE - bug-class fence
// ============================================================================
//
//  D-ExhaustNoGravity: Exhaust particles do not receive gravity.  A missing
//  `if (p.kind != ParticleKind::Exhaust)` guard around apply_gravity would
//  allow gravity to leak into exhaust smoke velocity each frame, causing
//  trails to curve downward instead of hanging in place.
//
//  This test provides a non-zero starting velocity and checks that it is
//  exactly preserved after the step (i.e. gravity was NOT added to it).
//  The exact value {0.1f, 0.0f, -0.1f} will differ from the correct
//  {0.1f, kGravityPerFrame, -0.1f} if the guard is missing.
// ============================================================================
TEST_CASE("AC-R20 (AC-Pexh): Exhaust particle velocity is exactly preserved across step - gravity not applied", "[entities][particle]") {
    // Given: Exhaust particle with non-zero velocity {0.1, 0.0, -0.1}, ttl=5
    // When:  step() called once
    // Then:  velocity is EXACTLY {0.1, 0.0, -0.1} (gravity not applied)
    //        position changed by {0.1, 0.0, -0.1} (existing velocity applied)
    //        ttl decremented to 4
    entities::Particle p{};
    p.position = Vec3{0.0f, 0.0f, 0.0f};
    p.velocity = Vec3{0.1f, 0.0f, -0.1f};
    p.ttl      = 5;
    p.kind     = entities::ParticleKind::Exhaust;

    entities::step(p);

    CAPTURE(p.velocity.x, p.velocity.y, p.velocity.z, physics::kGravityPerFrame);
    // Velocity must be EXACTLY the original (no gravity added)
    // Using margin=kParticleEps; if gravity leaked in, velocity.y == kGravityPerFrame
    // which is ~2.4e-4, well above the 1e-6 tolerance, so the test will fail
    // loudly on the wrong implementation.
    REQUIRE(p.velocity.x == Catch::Approx( 0.1f).margin(kParticleEps));
    REQUIRE(p.velocity.y == Catch::Approx( 0.0f).margin(kParticleEps));
    REQUIRE(p.velocity.z == Catch::Approx(-0.1f).margin(kParticleEps));

    // Position: original (0,0,0) + velocity (0.1, 0, -0.1) = (0.1, 0, -0.1)
    CAPTURE(p.position.x, p.position.y, p.position.z);
    REQUIRE(p.position.x == Catch::Approx( 0.1f).margin(kParticleEps));
    REQUIRE(p.position.y == Catch::Approx( 0.0f).margin(kParticleEps));
    REQUIRE(p.position.z == Catch::Approx(-0.1f).margin(kParticleEps));

    // TTL decremented
    REQUIRE(p.ttl == 4);
}

// ===========================================================================
// GROUP 8: Hygiene (AC-R80 compile-time + runtime, AC-R82, AC-R83)
// ===========================================================================
//
// AC-R80 (no-raylib static_assert) is at the top of this file, immediately
// after the #include "entities/particle.hpp".
//
// AC-R81: claude_lander_entities link list = core + physics + warnings only.
//   Verified by CMakeLists.txt; no runtime TEST_CASE needed.
//   The BUILD_GAME=OFF build exercises this invariant.

TEST_CASE("AC-R80: entities/particle header and library compile and link without raylib (BUILD_GAME=OFF)", "[entities][particle]") {
    // Given: this test file was compiled with BUILD_GAME=OFF (no raylib on path)
    // When:  it reaches this TEST_CASE at runtime
    // Then:  it ran - which means particle.hpp compiled and linked without raylib,
    //        satisfying AC-R80.
    SUCCEED("compilation and linkage without raylib succeeded - AC-R80 (particle) satisfied");
}

// ---------------------------------------------------------------------------
// AC-R82: static_assert(noexcept(step(particle)))
//   step(Particle&) must be declared noexcept.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R82: step(Particle&) is declared noexcept", "[entities][particle]") {
    // Given: a Particle object
    // When:  noexcept(step(p)) is evaluated at compile time
    // Then:  it is true
    entities::Particle p{};
    static_assert(noexcept(entities::step(p)),
        "AC-R82 VIOLATED: entities::step(Particle&) must be declared noexcept.");
    SUCCEED("static_assert(noexcept(step(particle))) passed - AC-R82 satisfied");
}

// ---------------------------------------------------------------------------
// AC-R83: std::is_aggregate_v<Particle> - Particle must be a plain aggregate.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R83: Particle is an aggregate type (no virtual, no user-declared constructors)", "[entities][particle]") {
    // Given: the Particle type
    // When:  std::is_aggregate_v<entities::Particle> is checked at compile time
    // Then:  it is true
    static_assert(std::is_aggregate_v<entities::Particle>,
        "AC-R83 VIOLATED: entities::Particle must be an aggregate (no virtual, "
        "no user-declared constructors).");
    SUCCEED("std::is_aggregate_v<Particle> is true - AC-R83 satisfied");
}

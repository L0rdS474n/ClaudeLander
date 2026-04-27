// tests/test_rock.cpp -- Pass 10 entities/rock tests.
//
// Covers AC-R01..R10 + AC-Rgrav bug-class fence.
// All tests tagged [entities][rock].
//
// === Determinism plan ===
// All tests are fully deterministic.  Rock{} is a POD aggregate with
// constexpr defaults.  step(Rock&) is a pure function of its input state
// (applies physics::apply_gravity then integrates position).  No clocks,
// no filesystem, no PRNG, no system calls.  AC-R07 exercises 1000 iterations
// and requires bit-identical results from two independent runs.
//
// === Y-DOWN gravity sign fence (AC-Rgrav / AC-R10) ===
// The project uses Y-DOWN convention: positive y = downward.
// apply_gravity ADDS kGravityPerFrame to vy, so velocity.y grows positive
// (downward) each step.  A Y-UP bug (subtracting) would produce negative vy.
// AC-R10 catches that class of error with an explicit > 0 assertion.
//
// === AC-R80 architecture hygiene ===
// RAYLIB_VERSION is defined only if raylib.h was transitively included.
// The static_assert below catches that at compile time.
//
// === Red-state expectation ===
// This file FAILS TO COMPILE (missing headers) until the implementer creates:
//   src/entities/rock.hpp
//   src/entities/rock.cpp
// That is the correct red state for Gate 2 delivery.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <array>
#include <cstddef>
#include <type_traits>

#include "core/vec3.hpp"
#include "core/matrix3.hpp"
#include "physics/kinematics.hpp"
#include "entities/rock.hpp"

// ---------------------------------------------------------------------------
// AC-R80: entities/rock.hpp must not pull in raylib, world, render, or
// platform headers.  RAYLIB_VERSION is defined by raylib.h; only present here
// if the #include above transitively included raylib.
// ---------------------------------------------------------------------------
#ifdef RAYLIB_VERSION
static_assert(false,
    "AC-R80 VIOLATED: entities/rock.hpp pulled in raylib.h "
    "(RAYLIB_VERSION is defined).  Remove the raylib dependency from "
    "src/entities/rock.hpp and src/entities/rock.cpp.");
#endif

// ---------------------------------------------------------------------------
// Tolerance constant
// ---------------------------------------------------------------------------
static constexpr float kRockEps = 1e-6f;

// ===========================================================================
// GROUP 1: Rock struct defaults (AC-R01)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R01: Rock{} defaults: position {0,-50,0}, velocity zero, identity
//         orientation, alive=true.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R01: Rock{} default-constructs with position={0,-50,0}, velocity={0,0,0}, orientation=identity, alive=true", "[entities][rock]") {
    // Given: a default-constructed Rock
    // When:  position, velocity, orientation, and alive are read
    // Then:
    //   position    == {0.0f, -50.0f, 0.0f}
    //   velocity    == {0.0f,   0.0f, 0.0f}
    //   orientation == identity()
    //   alive       == true
    const entities::Rock rock{};

    CAPTURE(rock.position.x, rock.position.y, rock.position.z);
    REQUIRE(rock.position.x == Catch::Approx(0.0f).margin(kRockEps));
    REQUIRE(rock.position.y == Catch::Approx(-50.0f).margin(kRockEps));
    REQUIRE(rock.position.z == Catch::Approx(0.0f).margin(kRockEps));

    CAPTURE(rock.velocity.x, rock.velocity.y, rock.velocity.z);
    REQUIRE(rock.velocity.x == Catch::Approx(0.0f).margin(kRockEps));
    REQUIRE(rock.velocity.y == Catch::Approx(0.0f).margin(kRockEps));
    REQUIRE(rock.velocity.z == Catch::Approx(0.0f).margin(kRockEps));

    const Mat3 I = identity();
    CAPTURE(rock.orientation.col[0].x, rock.orientation.col[0].y, rock.orientation.col[0].z);
    CAPTURE(rock.orientation.col[1].x, rock.orientation.col[1].y, rock.orientation.col[1].z);
    CAPTURE(rock.orientation.col[2].x, rock.orientation.col[2].y, rock.orientation.col[2].z);
    REQUIRE(rock.orientation.col[0].x == Catch::Approx(I.col[0].x).margin(kRockEps));
    REQUIRE(rock.orientation.col[0].y == Catch::Approx(I.col[0].y).margin(kRockEps));
    REQUIRE(rock.orientation.col[0].z == Catch::Approx(I.col[0].z).margin(kRockEps));
    REQUIRE(rock.orientation.col[1].x == Catch::Approx(I.col[1].x).margin(kRockEps));
    REQUIRE(rock.orientation.col[1].y == Catch::Approx(I.col[1].y).margin(kRockEps));
    REQUIRE(rock.orientation.col[1].z == Catch::Approx(I.col[1].z).margin(kRockEps));
    REQUIRE(rock.orientation.col[2].x == Catch::Approx(I.col[2].x).margin(kRockEps));
    REQUIRE(rock.orientation.col[2].y == Catch::Approx(I.col[2].y).margin(kRockEps));
    REQUIRE(rock.orientation.col[2].z == Catch::Approx(I.col[2].z).margin(kRockEps));

    REQUIRE(rock.alive == true);
}

// ===========================================================================
// GROUP 2: Vertex count (AC-R02)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R02: kRockVertices.size() == 6 == kRockVertexCount.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R02: kRockVertices.size() equals 6 and matches kRockVertexCount", "[entities][rock]") {
    // Given: kRockVertices is the constexpr vertex array
    // When:  its size is checked
    // Then:  size == 6 == kRockVertexCount
    REQUIRE(entities::kRockVertexCount == 6u);
    REQUIRE(entities::kRockVertices.size() == 6u);
    REQUIRE(entities::kRockVertices.size() == entities::kRockVertexCount);
}

// ===========================================================================
// GROUP 3: Face count (AC-R03)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R03: kRockFaceCount == 8.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R03: kRockFaceCount equals 8", "[entities][rock]") {
    // Given: kRockFaceCount declared in entities/rock.hpp
    // When:  its value is checked
    // Then:  kRockFaceCount == 8
    REQUIRE(entities::kRockFaceCount == 8u);
}

// ===========================================================================
// GROUP 4: Vertex values (AC-R04)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R04: Rock vertices are axis-aligned ±1 octahedron (six points: ±x, ±y, ±z).
//
// Expected table (from spec pass-10-rock-particles-spec.md §1):
//   { 1, 0, 0}   +x
//   {-1, 0, 0}   -x
//   { 0, 1, 0}   +y (down in Y-DOWN)
//   { 0,-1, 0}   -y (up)
//   { 0, 0, 1}   +z
//   { 0, 0,-1}   -z
//
// The array may be in any order; the test checks that all six axis-aligned
// unit-axis extrema are present (each appearing exactly once).
// ---------------------------------------------------------------------------
TEST_CASE("AC-R04: kRockVertices contains exactly the six axis-aligned ±1 octahedron points", "[entities][rock]") {
    // Given: kRockVertices is the vertex constant array
    // When:  each of the six expected axis-aligned unit extrema is searched for
    // Then:  each expected vertex is found exactly once within kRockEps

    struct ExpVtx { float x, y, z; const char* label; };
    static constexpr std::array<ExpVtx, 6> kExpected = {{
        { 1.0f,  0.0f,  0.0f, "+x"},
        {-1.0f,  0.0f,  0.0f, "-x"},
        { 0.0f,  1.0f,  0.0f, "+y"},
        { 0.0f, -1.0f,  0.0f, "-y"},
        { 0.0f,  0.0f,  1.0f, "+z"},
        { 0.0f,  0.0f, -1.0f, "-z"},
    }};

    for (const auto& exp : kExpected) {
        int found = 0;
        for (std::size_t i = 0; i < entities::kRockVertexCount; ++i) {
            const Vec3& v = entities::kRockVertices[i];
            if (std::abs(v.x - exp.x) <= kRockEps &&
                std::abs(v.y - exp.y) <= kRockEps &&
                std::abs(v.z - exp.z) <= kRockEps) {
                ++found;
            }
        }
        CAPTURE(exp.label, exp.x, exp.y, exp.z, found);
        REQUIRE(found == 1);
    }
}

// ===========================================================================
// GROUP 5: Step — gravity applied to velocity (AC-R05)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R05: step(rock) applies gravity to velocity (matches apply_gravity).
//   Starting from zero velocity, after one step velocity.y ==
//   apply_gravity({0,0,0}).y == kGravityPerFrame.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R05: step(rock) applies gravity to velocity (velocity.y increases by kGravityPerFrame)", "[entities][rock]") {
    // Given: rock with zero velocity, alive=true
    // When:  step() called once
    // Then:  velocity.y == kGravityPerFrame (exactly as apply_gravity would produce)
    //        velocity.x and velocity.z unchanged (== 0)
    entities::Rock rock{};
    rock.velocity = Vec3{0.0f, 0.0f, 0.0f};

    entities::step(rock);

    const float expected_vy = physics::kGravityPerFrame;
    CAPTURE(rock.velocity.x, rock.velocity.y, rock.velocity.z, expected_vy);
    REQUIRE(rock.velocity.x == Catch::Approx(0.0f).margin(kRockEps));
    REQUIRE(rock.velocity.y == Catch::Approx(expected_vy).margin(kRockEps));
    REQUIRE(rock.velocity.z == Catch::Approx(0.0f).margin(kRockEps));
}

// ===========================================================================
// GROUP 6: Step — position integration (AC-R06)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R06: step(rock) integrates position += new_velocity (post-gravity).
//   Hand-computed: starting at origin with zero velocity, after one step
//   position.y == kGravityPerFrame (velocity was 0, then became kGravityPerFrame,
//   and position += that new velocity).
// ---------------------------------------------------------------------------
TEST_CASE("AC-R06: step(rock) integrates position += post-gravity velocity", "[entities][rock]") {
    // Given: rock at position {0,0,0}, velocity {0,0,0}, alive=true
    // When:  step() called once
    // Then:
    //   velocity.y  == kGravityPerFrame
    //   position.y  == kGravityPerFrame  (position += post-gravity velocity)
    //   position.x  == 0, position.z == 0
    entities::Rock rock{};
    rock.position = Vec3{0.0f, 0.0f, 0.0f};
    rock.velocity = Vec3{0.0f, 0.0f, 0.0f};

    entities::step(rock);

    const float g = physics::kGravityPerFrame;
    CAPTURE(rock.position.x, rock.position.y, rock.position.z, g);
    REQUIRE(rock.position.x == Catch::Approx(0.0f).margin(kRockEps));
    REQUIRE(rock.position.y == Catch::Approx(g).margin(kRockEps));
    REQUIRE(rock.position.z == Catch::Approx(0.0f).margin(kRockEps));
}

// ===========================================================================
// GROUP 7: Determinism (AC-R07)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R07: 1000 steps produce bit-identical state across two independent runs.
//   Confirms no PRNG, no clock, no global mutable state inside step(Rock&).
// ---------------------------------------------------------------------------
TEST_CASE("AC-R07: 1000 step(rock) calls are deterministic — bit-identical to a fresh independent run", "[entities][rock]") {
    // Given: two Rock objects with identical initial state
    // When:  each is advanced 1000 steps independently
    // Then:  final position and velocity are bit-identical between the two runs
    auto make_rock = []() {
        entities::Rock r{};
        r.position = Vec3{1.0f, -50.0f, -2.0f};
        r.velocity = Vec3{0.1f,  0.0f,   0.05f};
        r.alive    = true;
        return r;
    };

    entities::Rock run_a = make_rock();
    entities::Rock run_b = make_rock();

    for (int i = 0; i < 1000; ++i) {
        entities::step(run_a);
        entities::step(run_b);
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

// ===========================================================================
// GROUP 8: Dead rock no-op (AC-R08)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R08: alive=false → step is a complete no-op (position, velocity, alive
//         are all unchanged).
// ---------------------------------------------------------------------------
TEST_CASE("AC-R08: step(rock) with alive=false is a complete no-op", "[entities][rock]") {
    // Given: rock with alive=false, arbitrary position and velocity
    // When:  step() called
    // Then:  position unchanged, velocity unchanged, alive still false
    entities::Rock rock{};
    rock.position = Vec3{3.0f, 10.0f, -7.0f};
    rock.velocity = Vec3{0.5f,  1.0f,  0.2f};
    rock.alive    = false;

    entities::step(rock);

    CAPTURE(rock.position.x, rock.position.y, rock.position.z);
    CAPTURE(rock.velocity.x, rock.velocity.y, rock.velocity.z);
    REQUIRE(rock.position.x == Catch::Approx(3.0f).margin(kRockEps));
    REQUIRE(rock.position.y == Catch::Approx(10.0f).margin(kRockEps));
    REQUIRE(rock.position.z == Catch::Approx(-7.0f).margin(kRockEps));
    REQUIRE(rock.velocity.x == Catch::Approx(0.5f).margin(kRockEps));
    REQUIRE(rock.velocity.y == Catch::Approx(1.0f).margin(kRockEps));
    REQUIRE(rock.velocity.z == Catch::Approx(0.2f).margin(kRockEps));
    REQUIRE(rock.alive == false);
}

// ===========================================================================
// GROUP 9: Accumulated velocity (AC-R09)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R09: After 100 steps from rest: velocity.y ≈ 100 * kGravityPerFrame.
//   No drag on rocks (D-RockPhysics); gravity accumulates linearly.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R09: after 100 steps from zero velocity, velocity.y ≈ 100 * kGravityPerFrame", "[entities][rock]") {
    // Given: rock with zero velocity, alive=true
    // When:  step() called 100 times
    // Then:  velocity.y ≈ 100 * kGravityPerFrame within kRockEps * 100 (float
    //        accumulation tolerance)
    entities::Rock rock{};
    rock.velocity = Vec3{0.0f, 0.0f, 0.0f};

    for (int i = 0; i < 100; ++i) {
        entities::step(rock);
    }

    const float expected_vy = 100.0f * physics::kGravityPerFrame;
    CAPTURE(rock.velocity.y, expected_vy);
    // Tolerance: 100 additions of kGravityPerFrame; float rounding is negligible
    // at this scale (1e-4 range), so kRockEps * 100 is generous enough.
    REQUIRE(rock.velocity.y == Catch::Approx(expected_vy).margin(kRockEps * 100.0f));
}

// ===========================================================================
// GROUP 10: Y-DOWN gravity sign fence (AC-R10 / AC-Rgrav)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R10 (AC-Rgrav): velocity.y is POSITIVE after one step from rest.
//
// ============================================================================
//  Y-DOWN GRAVITY SIGN FENCE — bug-class fence
// ============================================================================
//
//  The project uses Y-DOWN convention (positive y = downward).  Gravity must
//  ADD to vy on each step.  A developer writing `vy -= kGravityPerFrame`
//  (Y-UP convention) would produce negative vy; this test catches that class
//  of error immediately.
//
//  Correct:    step({vel=0, alive=true}).velocity.y == +kGravityPerFrame > 0
//  Bug (flip): step({vel=0, alive=true}).velocity.y == -kGravityPerFrame < 0
//
//  If this test fails: re-read D-GravitySemantics (Y-DOWN) in the spec.
//  The test is almost certainly correct; the implementation has the sign wrong.
// ============================================================================
TEST_CASE("AC-R10 (AC-Rgrav): after one step from rest, velocity.y is positive (Y-DOWN: gravity adds to downward velocity)", "[entities][rock]") {
    // Given: rock with zero velocity, alive=true
    // When:  step() called once
    // Then:  velocity.y > 0 (Y-DOWN: gravity must ADD to downward velocity)
    //        A sign-flip bug would produce velocity.y < 0 here.
    entities::Rock rock{};
    rock.velocity = Vec3{0.0f, 0.0f, 0.0f};
    rock.alive    = true;

    entities::step(rock);

    CAPTURE(rock.velocity.y, physics::kGravityPerFrame);
    REQUIRE(rock.velocity.y > 0.0f);
    REQUIRE(rock.velocity.y == Catch::Approx(physics::kGravityPerFrame).margin(kRockEps));
}

// ===========================================================================
// GROUP 11: Hygiene (AC-R80 compile-time + runtime, AC-R82, AC-R83)
// ===========================================================================
//
// AC-R80 (no-raylib static_assert) is at the top of this file, immediately
// after the #include "entities/rock.hpp".
//
// AC-R81: claude_lander_entities link list = core + physics + warnings only.
//   Verified by CMakeLists.txt; no runtime TEST_CASE needed.
//   The BUILD_GAME=OFF build exercises this invariant.

TEST_CASE("AC-R80: entities/rock header and library compile and link without raylib (BUILD_GAME=OFF)", "[entities][rock]") {
    // Given: this test file was compiled with BUILD_GAME=OFF (no raylib on path)
    // When:  it reaches this TEST_CASE at runtime
    // Then:  it ran — which means rock.hpp compiled and linked without raylib,
    //        satisfying AC-R80.
    SUCCEED("compilation and linkage without raylib succeeded — AC-R80 satisfied");
}

// ---------------------------------------------------------------------------
// AC-R82: static_assert(noexcept(step(rock)))
//   step(Rock&) must be declared noexcept.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R82: step(Rock&) is declared noexcept", "[entities][rock]") {
    // Given: a Rock object
    // When:  noexcept(step(rock)) is evaluated at compile time
    // Then:  it is true
    entities::Rock rock{};
    static_assert(noexcept(entities::step(rock)),
        "AC-R82 VIOLATED: entities::step(Rock&) must be declared noexcept.");
    SUCCEED("static_assert(noexcept(step(rock))) passed — AC-R82 satisfied");
}

// ---------------------------------------------------------------------------
// AC-R83: std::is_aggregate_v<Rock> — Rock must be a plain aggregate (no
//         virtual functions, no user-declared constructors).
// ---------------------------------------------------------------------------
TEST_CASE("AC-R83: Rock is an aggregate type (no virtual, no user-declared constructors)", "[entities][rock]") {
    // Given: the Rock type
    // When:  std::is_aggregate_v<entities::Rock> is checked at compile time
    // Then:  it is true
    static_assert(std::is_aggregate_v<entities::Rock>,
        "AC-R83 VIOLATED: entities::Rock must be an aggregate (no virtual, "
        "no user-declared constructors).");
    SUCCEED("std::is_aggregate_v<Rock> is true — AC-R83 satisfied");
}

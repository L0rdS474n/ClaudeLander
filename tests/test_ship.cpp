// tests/test_ship.cpp — Pass 4 entities/ship tests.
//
// Covers AC-S01..S04 (Ship defaults, vertex count + values, face count)
// and AC-S80 (hygiene).
// All tests tagged [entities][ship].
//
// === Determinism plan ===
// All tests are fully deterministic.  Ship{} is a POD aggregate with
// constexpr defaults.  kShipVertices is a constexpr array.  No clocks,
// no filesystem, no PRNG, no system calls.
//
// === Vertex encoding ===
// The 9 body-frame vertex coordinates from docs/research/pass-4-ship-kinematics-spec.md
// are encoded inline below for AC-S03.  Source: ARM 8.24 fixed-point values
// divided by 0x01000000 = 16777216.
//
// | # |   x      |    y      |    z    |
// | 0 |  1.000   |  0.3125   |  0.500  |
// | 1 |  1.000   |  0.3125   | -0.500  |
// | 2 |  0.000   |  0.625    | -0.931  |
// | 3 | -0.099   |  0.3125   |  0.000  |
// | 4 |  0.000   |  0.625    |  1.075  |
// | 5 | -0.900   | -0.531    |  0.000  |
// | 6 |  0.333   |  0.3125   |  0.250  |
// | 7 |  0.333   |  0.3125   | -0.250  |
// | 8 | -0.800   |  0.3125   |  0.000  |
//
// === AC-S80 (architecture hygiene: no forbidden includes in ship.hpp) ===
// Same pattern as AC-K80: RAYLIB_VERSION static_assert immediately after include.
//
// === Red-state expectation ===
// This file FAILS TO COMPILE (missing headers) until the implementer creates:
//   src/entities/ship.hpp
//   src/entities/ship.cpp
//   src/physics/kinematics.hpp   (for ThrustLevel declaration)
// That is the correct red state for Gate 2 delivery.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <array>
#include <cstddef>

#include "core/vec3.hpp"
#include "core/matrix3.hpp"
#include "physics/kinematics.hpp"
#include "entities/ship.hpp"

// ---------------------------------------------------------------------------
// AC-S80: entities/ship.hpp must not pull in raylib, world, render, or
// platform headers.  RAYLIB_VERSION is defined by raylib.h; only present here
// if the #include above transitively included raylib.
// ---------------------------------------------------------------------------
#ifdef RAYLIB_VERSION
static_assert(false,
    "AC-S80 VIOLATED: entities/ship.hpp pulled in raylib.h "
    "(RAYLIB_VERSION is defined).  Remove the raylib dependency from "
    "src/entities/ship.hpp and src/entities/ship.cpp.");
#endif

// ---------------------------------------------------------------------------
// Tolerance constant
// ---------------------------------------------------------------------------
static constexpr float kVertexEps = 5e-4f;   // vertex coordinate check tolerance

// ===========================================================================
// GROUP 1: Ship struct defaults (AC-S01)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-S01: Ship{} defaults match D-InitialState exactly.
//   position = {0, 5, 0}, velocity = {0, 0, 0}, orientation = identity()
// ---------------------------------------------------------------------------
TEST_CASE("AC-S01: Ship{} default-constructs with position={0,5,0}, velocity={0,0,0}, orientation=identity", "[entities][ship]") {
    // Given: a default-constructed Ship
    // When:  position, velocity, and orientation are read
    // Then:
    //   position == {0.0f, 5.0f, 0.0f}
    //   velocity == {0.0f, 0.0f, 0.0f}
    //   orientation == identity()
    const entities::Ship ship{};
    static constexpr float kEps = 1e-6f;

    // Position
    CAPTURE(ship.position.x, ship.position.y, ship.position.z);
    REQUIRE(ship.position.x == Catch::Approx(0.0f).margin(kEps));
    REQUIRE(ship.position.y == Catch::Approx(5.0f).margin(kEps));
    REQUIRE(ship.position.z == Catch::Approx(0.0f).margin(kEps));

    // Velocity
    CAPTURE(ship.velocity.x, ship.velocity.y, ship.velocity.z);
    REQUIRE(ship.velocity.x == Catch::Approx(0.0f).margin(kEps));
    REQUIRE(ship.velocity.y == Catch::Approx(0.0f).margin(kEps));
    REQUIRE(ship.velocity.z == Catch::Approx(0.0f).margin(kEps));

    // Orientation == identity(): each column checked componentwise
    const Mat3 I = identity();
    CAPTURE(ship.orientation.col[0].x, ship.orientation.col[0].y, ship.orientation.col[0].z);
    CAPTURE(ship.orientation.col[1].x, ship.orientation.col[1].y, ship.orientation.col[1].z);
    CAPTURE(ship.orientation.col[2].x, ship.orientation.col[2].y, ship.orientation.col[2].z);

    // col[0] = {1,0,0}
    REQUIRE(ship.orientation.col[0].x == Catch::Approx(I.col[0].x).margin(kEps));
    REQUIRE(ship.orientation.col[0].y == Catch::Approx(I.col[0].y).margin(kEps));
    REQUIRE(ship.orientation.col[0].z == Catch::Approx(I.col[0].z).margin(kEps));
    // col[1] = {0,1,0}
    REQUIRE(ship.orientation.col[1].x == Catch::Approx(I.col[1].x).margin(kEps));
    REQUIRE(ship.orientation.col[1].y == Catch::Approx(I.col[1].y).margin(kEps));
    REQUIRE(ship.orientation.col[1].z == Catch::Approx(I.col[1].z).margin(kEps));
    // col[2] = {0,0,1}
    REQUIRE(ship.orientation.col[2].x == Catch::Approx(I.col[2].x).margin(kEps));
    REQUIRE(ship.orientation.col[2].y == Catch::Approx(I.col[2].y).margin(kEps));
    REQUIRE(ship.orientation.col[2].z == Catch::Approx(I.col[2].z).margin(kEps));
}

// ===========================================================================
// GROUP 2: Vertex count (AC-S02)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-S02: kShipVertices.size() == 9 == kShipVertexCount
// ---------------------------------------------------------------------------
TEST_CASE("AC-S02: kShipVertices.size() equals 9 and matches kShipVertexCount", "[entities][ship]") {
    // Given: kShipVertices is the constexpr vertex array
    // When:  its size is checked
    // Then:  size == 9 == kShipVertexCount
    REQUIRE(entities::kShipVertexCount == 9u);
    REQUIRE(entities::kShipVertices.size() == 9u);
    REQUIRE(entities::kShipVertices.size() == entities::kShipVertexCount);
}

// ===========================================================================
// GROUP 3: Vertex values (AC-S03)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-S03: All 9 vertices match spec table within kVertexEps = 5e-4f.
//
// Source: docs/research/pass-4-ship-kinematics-spec.md §Ship mesh (9 vertices)
// Values derived from ARM 8.24 fixed-point constants / 0x01000000.
//
// The test checks each of the 9 vertices componentwise.  Any transposition,
// sign error, or off-by-one in the vertex list will fail a specific
// REQUIRE with the affected vertex index captured.
// ---------------------------------------------------------------------------
TEST_CASE("AC-S03: all 9 ship vertices match spec table within 5e-4", "[entities][ship]") {
    // Given: kShipVertices — the body-frame vertex constant array
    // When:  each of the 9 vertices is checked against the spec table
    // Then:  every component is within kVertexEps = 5e-4f of the expected value

    // Expected values from pass-4-ship-kinematics-spec.md:
    struct ExpectedVertex { float x, y, z; };
    static constexpr std::array<ExpectedVertex, 9> kExpected = {{
        {  1.000f,  0.3125f,  0.500f },  // vertex 0
        {  1.000f,  0.3125f, -0.500f },  // vertex 1
        {  0.000f,  0.625f,  -0.931f },  // vertex 2
        { -0.099f,  0.3125f,  0.000f },  // vertex 3
        {  0.000f,  0.625f,   1.075f },  // vertex 4
        { -0.900f, -0.531f,   0.000f },  // vertex 5
        {  0.333f,  0.3125f,  0.250f },  // vertex 6
        {  0.333f,  0.3125f, -0.250f },  // vertex 7
        { -0.800f,  0.3125f,  0.000f },  // vertex 8
    }};

    for (std::size_t i = 0; i < 9u; ++i) {
        const Vec3& v   = entities::kShipVertices[i];
        const auto& exp = kExpected[i];
        CAPTURE(i, v.x, v.y, v.z, exp.x, exp.y, exp.z);
        REQUIRE(v.x == Catch::Approx(exp.x).margin(kVertexEps));
        REQUIRE(v.y == Catch::Approx(exp.y).margin(kVertexEps));
        REQUIRE(v.z == Catch::Approx(exp.z).margin(kVertexEps));
    }
}

// ===========================================================================
// GROUP 4: Face count (AC-S04)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-S04: kShipFaceCount == 9 (face index list DEFERRED to Pass 6).
//   Only the count is verified here; the actual face index triples are
//   out of scope for Pass 4.
// ---------------------------------------------------------------------------
TEST_CASE("AC-S04: kShipFaceCount equals 9 (face index list deferred to Pass 6)", "[entities][ship]") {
    // Given: kShipFaceCount declared in entities/ship.hpp
    // When:  its value is checked
    // Then:  kShipFaceCount == 9
    REQUIRE(entities::kShipFaceCount == 9u);
}

// ===========================================================================
// GROUP 5: Hygiene (AC-S80)
// ===========================================================================
//
// AC-S80 (no-raylib static_assert) is at the top of this file, immediately
// after the #include "entities/ship.hpp".

TEST_CASE("AC-S80: entities/ship header and library compile and link without raylib (BUILD_GAME=OFF)", "[entities][ship]") {
    // Given: this test file was compiled with BUILD_GAME=OFF (no raylib on path)
    // When:  it reaches this TEST_CASE at runtime
    // Then:  it ran — which means ship.hpp compiled and linked without raylib,
    //        satisfying AC-S80.
    SUCCEED("compilation and linkage without raylib succeeded — AC-S80 satisfied");
}

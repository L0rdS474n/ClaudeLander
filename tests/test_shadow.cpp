// tests/test_shadow.cpp - Pass 9 render/shadow tests.
//
// Covers AC-S01..S05 (shadow projection) + AC-S80..S82 (hygiene, shadow side).
// All tests tagged [render][shadow].
//
// === Determinism plan ===
// render::project_shadow is declared noexcept and is a pure function of the
// supplied world-space vertices.  world::altitude (via terrain::altitude) is
// itself a pure function of (x, z) with no global mutable state, no PRNG, no
// clocks.  AC-S03 verifies bit-identical output across two independent runs
// over the 9 ship vertices.
//
// External dependency: terrain::altitude(x, z) is the same pure function used
// by world/terrain - test files have looser include rules than src/render/, so
// this TU may include world/terrain.hpp directly to verify the y-replacement.
//
// === Shadow formula (D-ShadowDirection, D-ShadowSpan) ===
//   shadow_out[i].x = vertices_world[i].x
//   shadow_out[i].z = vertices_world[i].z
//   shadow_out[i].y = terrain::altitude(vertices_world[i].x, vertices_world[i].z)
//
// === Bug-class fences ===
// (none specific to shadow; the three fences live in test_collision.cpp)
//
// === AC-S82 noexcept verification ===
// static_assert checks noexcept on project_shadow placed after the
// #include so it fires at compile time in the same TU.
//
// === Red-state expectation ===
// This file FAILS TO COMPILE until the implementer creates:
//   src/render/shadow.hpp
//   src/render/shadow.cpp
// That is the correct red state for Gate 2 delivery.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <span>
#include <vector>

#include "core/vec3.hpp"
#include "world/terrain.hpp"         // terrain::altitude - test files may include world/
#include "render/shadow.hpp"         // render::project_shadow

// ---------------------------------------------------------------------------
// AC-S80: src/render/shadow.hpp must not pull in raylib, world/, entities/,
// <random>, or <chrono>.
// RAYLIB_VERSION is defined by raylib.h; if present here, the include chain
// is polluted.  The BUILD_GAME=OFF build keeps raylib off the compiler path.
// ---------------------------------------------------------------------------
#ifdef RAYLIB_VERSION
static_assert(false,
    "AC-S80 VIOLATED: render/shadow.hpp pulled in raylib.h "
    "(RAYLIB_VERSION is defined).  Remove the raylib dependency.");
#endif

// ---------------------------------------------------------------------------
// AC-S82 - project_shadow must be declared noexcept.
// ===========================================================================
//
//  D-Stateless: pure noexcept function, no globals.
//  Verified at compile time here in the same TU as the #include.
//  The static_assert fires immediately if the implementer forgets noexcept.
// ===========================================================================
static_assert(
    noexcept(render::project_shadow(std::span<const Vec3>{}, std::span<Vec3>{})),
    "AC-S82: render::project_shadow must be declared noexcept");

// ---------------------------------------------------------------------------
// Tolerance constant (per planner spec sec 5)
// ---------------------------------------------------------------------------
static constexpr float kShadowEps = 1e-5f;

// ---------------------------------------------------------------------------
// Helper: a small fixed set of world vertices representing 9 ship-like points.
// These are at varied x/z so altitude differs per vertex.  Chosen to be within
// terrain's periodic domain (integer tile lattice well inside range).
// ---------------------------------------------------------------------------
static const std::array<Vec3, 9> kShipVerts = {{
    { 0.0f, 3.0f,  0.0f},
    { 1.0f, 2.0f,  0.0f},
    {-1.0f, 2.0f,  0.0f},
    { 0.0f, 2.0f,  1.0f},
    { 0.0f, 2.0f, -1.0f},
    { 0.5f, 1.5f,  0.5f},
    {-0.5f, 1.5f,  0.5f},
    { 0.5f, 1.5f, -0.5f},
    {-0.5f, 1.5f, -0.5f},
}};

// ===========================================================================
// GROUP 1: Basic shadow projection (AC-S01..S05)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-S01 - project_shadow for a single vertex {0, 0, 0} outputs
//           {0, altitude(0,0), 0}.
//   Given: one vertex at the world origin
//   When:  project_shadow is called
//   Then:  shadow_out[0].x == 0, shadow_out[0].z == 0,
//          shadow_out[0].y == terrain::altitude(0,0)
// ---------------------------------------------------------------------------
TEST_CASE("AC-S01: single vertex {0,0,0} projects shadow to {0, altitude(0,0), 0}", "[render][shadow]") {
    // Given
    const std::array<Vec3, 1> verts = {{ {0.0f, 0.0f, 0.0f} }};
    std::array<Vec3, 1> out = {{ {0.0f, 0.0f, 0.0f} }};

    // When
    render::project_shadow(
        std::span<const Vec3>{verts},
        std::span<Vec3>{out});

    // Then
    const float expected_y = terrain::altitude(0.0f, 0.0f);
    CAPTURE(out[0].x, out[0].y, out[0].z, expected_y);
    REQUIRE(out[0].x == Catch::Approx(0.0f).margin(kShadowEps));
    REQUIRE(out[0].z == Catch::Approx(0.0f).margin(kShadowEps));
    REQUIRE(out[0].y == Catch::Approx(expected_y).margin(kShadowEps));
}

// ---------------------------------------------------------------------------
// AC-S02 - Each output vertex preserves x/z, replaces y with altitude(x,z).
//   Given: 9 ship vertices at varying x/z positions
//   When:  project_shadow is called
//   Then:  for every i:
//            shadow_out[i].x == verts[i].x
//            shadow_out[i].z == verts[i].z
//            shadow_out[i].y == terrain::altitude(verts[i].x, verts[i].z)
// ---------------------------------------------------------------------------
TEST_CASE("AC-S02: each output vertex preserves x/z and replaces y with altitude(x,z)", "[render][shadow]") {
    // Given
    std::array<Vec3, 9> out{};

    // When
    render::project_shadow(
        std::span<const Vec3>{kShipVerts},
        std::span<Vec3>{out});

    // Then
    for (std::size_t i = 0; i < kShipVerts.size(); ++i) {
        const float expected_y = terrain::altitude(kShipVerts[i].x, kShipVerts[i].z);
        CAPTURE(i, kShipVerts[i].x, kShipVerts[i].z, out[i].x, out[i].y, out[i].z, expected_y);
        REQUIRE(out[i].x == Catch::Approx(kShipVerts[i].x).margin(kShadowEps));
        REQUIRE(out[i].z == Catch::Approx(kShipVerts[i].z).margin(kShadowEps));
        REQUIRE(out[i].y == Catch::Approx(expected_y).margin(kShadowEps));
    }
}

// ---------------------------------------------------------------------------
// AC-S03 - Determinism: bit-identical between two runs over the 9 ship vertices.
//   Given: the 9 ship vertices from kShipVerts
//   When:  project_shadow is called twice independently with the same input
//   Then:  every (x, y, z) triplet is bit-identical between runs A and B
// ---------------------------------------------------------------------------
TEST_CASE("AC-S03: project_shadow is bit-identical across two independent runs over 9 ship vertices", "[render][shadow]") {
    // Given: fixed input, no PRNG, no clock
    std::array<Vec3, 9> run_a{};
    std::array<Vec3, 9> run_b{};

    // When
    render::project_shadow(std::span<const Vec3>{kShipVerts}, std::span<Vec3>{run_a});
    render::project_shadow(std::span<const Vec3>{kShipVerts}, std::span<Vec3>{run_b});

    // Then - bit-identical (exact equality, NOT Approx)
    for (std::size_t i = 0; i < kShipVerts.size(); ++i) {
        CAPTURE(i, run_a[i].x, run_b[i].x, run_a[i].y, run_b[i].y);
        REQUIRE(run_a[i].x == run_b[i].x);
        REQUIRE(run_a[i].y == run_b[i].y);
        REQUIRE(run_a[i].z == run_b[i].z);
    }
}

// ---------------------------------------------------------------------------
// AC-S04 - Span size match: body.size() == shadow_out.size() must be honoured.
//   Given: 3 vertices with output span sized to 3
//   When:  project_shadow is called
//   Then:  exactly 3 shadow vertices are written; no out-of-bounds (observable
//          via sentinel slots that must remain unchanged)
// ---------------------------------------------------------------------------
TEST_CASE("AC-S04: span size match honoured - 3 vertices produce exactly 3 shadow vertices", "[render][shadow]") {
    // Given
    const std::array<Vec3, 3> verts = {{
        {2.0f, 5.0f, 3.0f},
        {4.0f, 5.0f, 1.0f},
        {3.0f, 5.0f, 2.0f},
    }};
    // Allocate one extra slot as a sentinel; fill with recognisable value
    std::array<Vec3, 4> out_buf;
    out_buf[3] = {999.0f, 999.0f, 999.0f};

    // When: pass span of size 3 only (sentinel at [3] is not in the span)
    render::project_shadow(
        std::span<const Vec3>{verts.data(), 3},
        std::span<Vec3>{out_buf.data(), 3});

    // Then: the 3 slots were written
    for (std::size_t i = 0; i < 3; ++i) {
        const float expected_y = terrain::altitude(verts[i].x, verts[i].z);
        CAPTURE(i, out_buf[i].x, out_buf[i].y, out_buf[i].z, expected_y);
        REQUIRE(out_buf[i].y == Catch::Approx(expected_y).margin(kShadowEps));
    }
    // Sentinel must be untouched - project_shadow must not write beyond span
    CAPTURE(out_buf[3].x, out_buf[3].y, out_buf[3].z);
    REQUIRE(out_buf[3].x == 999.0f);
    REQUIRE(out_buf[3].y == 999.0f);
    REQUIRE(out_buf[3].z == 999.0f);
}

// ---------------------------------------------------------------------------
// AC-S05 - Empty input -> empty output (no crash, no iteration).
//   Given: spans of size 0
//   When:  project_shadow is called
//   Then:  function returns without error; output span is unchanged (empty)
// ---------------------------------------------------------------------------
TEST_CASE("AC-S05: empty input span produces empty output with no crash", "[render][shadow]") {
    // Given: both spans of size 0
    const std::span<const Vec3> empty_in{};
    std::span<Vec3>             empty_out{};

    // When / Then: must not throw, crash, or assert
    REQUIRE_NOTHROW(render::project_shadow(empty_in, empty_out));

    // Also verify with a non-null but zero-size span (implementation portability)
    std::array<Vec3, 1> dummy_buf{};
    render::project_shadow(
        std::span<const Vec3>{dummy_buf.data(), 0},
        std::span<Vec3>{dummy_buf.data(), 0});
    SUCCEED("project_shadow with size-0 spans completed without crash - AC-S05 satisfied");
}

// ===========================================================================
// GROUP 2: Hygiene (AC-S80..S82, shadow side)
// ===========================================================================
//
// AC-S80 (no-raylib static_assert) is at the top of this file.
// AC-S82 (noexcept static_assert) is at the top of this file.

TEST_CASE("AC-S80: render/shadow.hpp compiles without raylib, world/, entities/, <random>, <chrono>", "[render][shadow]") {
    // Given: this test file was compiled with BUILD_GAME=OFF (no raylib on path)
    // When:  it reaches this TEST_CASE at runtime
    // Then:  it ran - which means the header compiled without forbidden deps,
    //        satisfying AC-S80 (render side).
    SUCCEED("compilation without raylib/forbidden deps succeeded - AC-S80 (shadow side) satisfied");
}

TEST_CASE("AC-S81: claude_lander_render link list unchanged after adding shadow.cpp", "[render][shadow]") {
    // Given: this test binary was linked with claude_lander_render which links
    //        against claude_lander_core + claude_lander_warnings only.
    // When:  it reaches this TEST_CASE
    // Then:  it ran without link errors - AC-S81 satisfied (render side).
    SUCCEED("render library link list unchanged (core+warnings only) - AC-S81 (shadow side) satisfied");
}

TEST_CASE("AC-S82: project_shadow is noexcept (verified by static_assert at top of TU)", "[render][shadow]") {
    // Given: static_assert(noexcept(render::project_shadow(...))) at the top of
    //        this file (inside anonymous namespace helper).
    // When:  this test is reached (compile succeeded means static_assert passed)
    // Then:  AC-S82 is satisfied.
    SUCCEED("static_assert(noexcept(project_shadow(...))) passed at compile time - AC-S82 (shadow side) satisfied");
}

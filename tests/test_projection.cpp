// tests/test_projection.cpp - Pass 3 projection tests.
//
// Covers AC-R01..R82 (32 ACs across 9 groups).
// All tests tagged [render][projection].  No [.golden] - every expected pixel
// is hand-derivable from the formulas:
//
//   x_screen = kScreenCenterX + x_c / z_c   (160 + x/z)
//   y_screen = kScreenCenterY + y_c / z_c   (64  + y/z)
//
// where (x_c, y_c, z_c) = world_point - camera.position.
//
// === Determinism plan ===
// All tests are fully deterministic.  render::project() is a pure function
// of (world_point, camera).  No clocks, no PRNG, no filesystem, no system
// calls.  The determinism test (AC-R70) calls project() 1000 times and
// requires bit-identical results on every call.
//
// === Y-FLIP design awareness ===
// D-Yflip (docs/plans/pass-3-projection.md) locks: NO y-negation is applied.
// World Y-DOWN == raylib 2D screen Y-DOWN.  AC-R20..R22 are the loud guard.
// See the banner comment above those tests for the full rationale.
//
// === AC-R80 (architecture hygiene: no raylib in projection.hpp) ===
// Implemented as a conditional static_assert: if RAYLIB_VERSION is defined
// (which only happens when raylib.h was included transitively), compilation
// fails with a clear message.  The BUILD_GAME=OFF build guarantees raylib is
// not on the include path; the static_assert is an additional in-TU tripwire.
//
// === Red-state expectation ===
// This file FAILS TO COMPILE (missing headers) until the implementer creates:
//   src/core/vec2.hpp
//   src/render/projection.hpp
// That is the correct red state for Gate 2 delivery.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <cstddef>
#include <optional>

#include "core/vec2.hpp"       // Vec2, approx_equal(Vec2, Vec2, float)
#include "render/projection.hpp"  // render::Camera, render::project,
                                  // render::kScreenCenterX, render::kScreenCenterY,
                                  // render::kNearCullZ,    render::kFarCullZ,
                                  // render::kLogicalScreenW, render::kLogicalScreenH

// ---------------------------------------------------------------------------
// AC-R80: render/projection.hpp must NOT pull in raylib.
// RAYLIB_VERSION is defined by raylib.h; it is only present here if the
// #include above transitively included raylib.  The BUILD_GAME=OFF build
// already keeps raylib off the compiler include path - this static_assert
// is a belt-and-suspenders in-translation-unit tripwire.
// ---------------------------------------------------------------------------
#ifdef RAYLIB_VERSION
static_assert(false,
    "AC-R80 VIOLATED: render/projection.hpp pulled in raylib.h "
    "(RAYLIB_VERSION is defined).  Remove the raylib dependency from "
    "src/render/projection.hpp and src/render/projection.cpp.");
#endif

// ---------------------------------------------------------------------------
// Tolerance constants (per planner spec §4)
// ---------------------------------------------------------------------------
static constexpr float kProjEps     = 1e-5f;   // pixel equality checks
static constexpr float kBoundaryEps = 1e-4f;   // boundary step for near/far cull

// ===========================================================================
// GROUP 1: Vec2 + Camera struct (AC-R01..R04)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R01: Vec2 members are readable as .x and .y
// ---------------------------------------------------------------------------
TEST_CASE("AC-R01: Vec2{1,2} has x==1.0f and y==2.0f", "[render][projection]") {
    // Given: a Vec2 initialised with x=1.0f, y=2.0f
    // When:  .x and .y are read back
    // Then:  .x == 1.0f exactly and .y == 2.0f exactly
    const Vec2 v{1.0f, 2.0f};
    REQUIRE(v.x == 1.0f);
    REQUIRE(v.y == 2.0f);
}

// ---------------------------------------------------------------------------
// AC-R02: Camera stores its Vec3 position and makes it readable
// ---------------------------------------------------------------------------
TEST_CASE("AC-R02: Camera{Vec3{1,2,3}} stores position == (1,2,3)", "[render][projection]") {
    // Given: a Camera constructed with Vec3{1,2,3}
    // When:  .position is read back
    // Then:  .position.x==1, .position.y==2, .position.z==3
    const render::Camera cam{Vec3{1.0f, 2.0f, 3.0f}};
    REQUIRE(cam.position.x == 1.0f);
    REQUIRE(cam.position.y == 2.0f);
    REQUIRE(cam.position.z == 3.0f);
}

// ---------------------------------------------------------------------------
// AC-R03: Camera default-constructs with position == (0,0,0)
// ---------------------------------------------------------------------------
TEST_CASE("AC-R03: Camera{} default-constructs with position (0,0,0)", "[render][projection]") {
    // Given: a default-constructed Camera
    // When:  .position is read
    // Then:  all components are 0.0f
    const render::Camera cam{};
    REQUIRE(cam.position.x == 0.0f);
    REQUIRE(cam.position.y == 0.0f);
    REQUIRE(cam.position.z == 0.0f);
}

// ---------------------------------------------------------------------------
// AC-R04: approx_equal(Vec2, Vec2) returns true for identical, false when
//         component differs beyond epsilon
// ---------------------------------------------------------------------------
TEST_CASE("AC-R04: approx_equal(Vec2) returns true for exact match and false when outside eps", "[render][projection]") {
    // Given: approx_equal(Vec2{1,2}, Vec2{1,2}, 1e-5f)
    // When:  called with identical values
    // Then:  returns true
    REQUIRE(approx_equal(Vec2{1.0f, 2.0f}, Vec2{1.0f, 2.0f}, 1e-5f));

    // Given: approx_equal(Vec2{1,2}, Vec2{1.001,2}, 1e-5f)
    // When:  x differs by 0.001, well outside 1e-5f
    // Then:  returns false
    REQUIRE_FALSE(approx_equal(Vec2{1.0f, 2.0f}, Vec2{1.001f, 2.0f}, 1e-5f));

    // Edge: difference exactly at eps boundary - equal
    REQUIRE(approx_equal(Vec2{1.0f, 2.0f}, Vec2{1.0f + 1e-5f, 2.0f}, 1e-5f));
}

// ===========================================================================
// GROUP 2: Identity / degenerate inputs (AC-R05..R07)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R05: world_point == camera.position → z_cam = 0 → culled (nullopt)
// ---------------------------------------------------------------------------
TEST_CASE("AC-R05: point at camera position is culled (z_cam = 0 fails near-cull)", "[render][projection]") {
    // Given: camera at (3,7,11), world_point == (3,7,11)
    // When:  project() is called
    // Then:  z_c = 0 which is <= kNearCullZ, so result is nullopt
    const render::Camera cam{Vec3{3.0f, 7.0f, 11.0f}};
    const auto result = render::project(Vec3{3.0f, 7.0f, 11.0f}, cam);
    REQUIRE_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// AC-R06: camera at origin, point (0,0,10) → screen centre (160,64)
// ---------------------------------------------------------------------------
TEST_CASE("AC-R06: (0,0,10) with origin camera projects to screen centre (160,64)", "[render][projection]") {
    // Given: camera at origin, world_point = (0,0,10)
    // When:  project() is called
    // Then:  x_screen = 160 + 0/10 = 160,  y_screen = 64 + 0/10 = 64
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, 0.0f, 10.0f}, cam);
    REQUIRE(result.has_value());
    REQUIRE(result->x == Catch::Approx(160.0f).margin(kProjEps));
    REQUIRE(result->y == Catch::Approx(64.0f).margin(kProjEps));
}

// ---------------------------------------------------------------------------
// AC-R07: point with z_cam < kNearCullZ (half near-plane) → culled
// ---------------------------------------------------------------------------
TEST_CASE("AC-R07: point with z_cam = kNearCullZ * 0.5f is culled", "[render][projection]") {
    // Given: camera at origin, world_point z = kNearCullZ * 0.5f (< kNearCullZ)
    // When:  project() is called
    // Then:  z_c = kNearCullZ * 0.5f <= kNearCullZ, so nullopt
    const render::Camera cam{};
    const float z = render::kNearCullZ * 0.5f;
    const auto result = render::project(Vec3{0.0f, 0.0f, z}, cam);
    REQUIRE_FALSE(result.has_value());
}

// ===========================================================================
// GROUP 3: On-axis projection (AC-R10..R14)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R10: (0,0,10) → (160, 64)
//   x_screen = 160 + 0/10 = 160.0
//   y_screen =  64 + 0/10 =  64.0
// ---------------------------------------------------------------------------
TEST_CASE("AC-R10: (0,0,10) projects to screen centre (160, 64)", "[render][projection]") {
    // Given: camera at origin, world_point = (0,0,10)
    // When:  project() is called
    // Then:  (160.0, 64.0) within kProjEps
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, 0.0f, 10.0f}, cam);
    REQUIRE(result.has_value());
    CAPTURE(result->x, result->y);
    REQUIRE(result->x == Catch::Approx(160.0f).margin(kProjEps));
    REQUIRE(result->y == Catch::Approx(64.0f).margin(kProjEps));
}

// ---------------------------------------------------------------------------
// AC-R11: (0,0,100) → (160, 64)
//   x_screen = 160 + 0/100 = 160.0
//   y_screen =  64 + 0/100 =  64.0
// ---------------------------------------------------------------------------
TEST_CASE("AC-R11: (0,0,100) projects to screen centre (160, 64)", "[render][projection]") {
    // Given: camera at origin, world_point = (0,0,100)
    // When:  project() is called
    // Then:  (160.0, 64.0) within kProjEps
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, 0.0f, 100.0f}, cam);
    REQUIRE(result.has_value());
    CAPTURE(result->x, result->y);
    REQUIRE(result->x == Catch::Approx(160.0f).margin(kProjEps));
    REQUIRE(result->y == Catch::Approx(64.0f).margin(kProjEps));
}

// ---------------------------------------------------------------------------
// AC-R12: (0,0,1.0f) → (160, 64)
//   x_screen = 160 + 0/1 = 160.0
//   y_screen =  64 + 0/1 =  64.0
// ---------------------------------------------------------------------------
TEST_CASE("AC-R12: (0,0,1) projects to screen centre (160, 64)", "[render][projection]") {
    // Given: camera at origin, world_point = (0,0,1)
    // When:  project() is called
    // Then:  (160.0, 64.0) within kProjEps
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, 0.0f, 1.0f}, cam);
    REQUIRE(result.has_value());
    CAPTURE(result->x, result->y);
    REQUIRE(result->x == Catch::Approx(160.0f).margin(kProjEps));
    REQUIRE(result->y == Catch::Approx(64.0f).margin(kProjEps));
}

// ---------------------------------------------------------------------------
// AC-R13: (0,0,999999.0f) → (160, 64)
//   x_screen = 160 + 0/999999 ≈ 160.0
//   y_screen =  64 + 0/999999 ≈  64.0
// ---------------------------------------------------------------------------
TEST_CASE("AC-R13: (0,0,999999) projects to screen centre (160, 64)", "[render][projection]") {
    // Given: camera at origin, world_point = (0,0,999999)
    // When:  project() is called
    // Then:  (160.0, 64.0) within kProjEps (on-axis, below far-cull limit)
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, 0.0f, 999999.0f}, cam);
    REQUIRE(result.has_value());
    CAPTURE(result->x, result->y);
    REQUIRE(result->x == Catch::Approx(160.0f).margin(kProjEps));
    REQUIRE(result->y == Catch::Approx(64.0f).margin(kProjEps));
}

// ---------------------------------------------------------------------------
// AC-R14: (0,0, kFarCullZ * 0.99f) → returns Some (just inside far boundary)
// ---------------------------------------------------------------------------
TEST_CASE("AC-R14: point just inside far-cull limit returns Some", "[render][projection]") {
    // Given: camera at origin, world_point z = kFarCullZ * 0.99f (< kFarCullZ)
    // When:  project() is called
    // Then:  result has_value() (not culled)
    const render::Camera cam{};
    const float z = render::kFarCullZ * 0.99f;
    const auto result = render::project(Vec3{0.0f, 0.0f, z}, cam);
    REQUIRE(result.has_value());
}

// ===========================================================================
// GROUP 4: Off-axis projection (AC-R15..R18)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R15: (100,0,10) → (170, 64)
//   x_screen = 160 + 100/10 = 160 + 10 = 170.0
//   y_screen =  64 +   0/10 =  64 +  0 =  64.0
// ---------------------------------------------------------------------------
TEST_CASE("AC-R15: (100,0,10) projects to (170, 64) - x-positive offset", "[render][projection]") {
    // Given: camera at origin, world_point = (100, 0, 10)
    // When:  project() is called
    // Then:  x_screen = 160 + 100/10 = 170.0,  y_screen = 64 + 0/10 = 64.0
    const render::Camera cam{};
    const auto result = render::project(Vec3{100.0f, 0.0f, 10.0f}, cam);
    REQUIRE(result.has_value());
    CAPTURE(result->x, result->y);
    REQUIRE(result->x == Catch::Approx(170.0f).margin(kProjEps));
    REQUIRE(result->y == Catch::Approx(64.0f).margin(kProjEps));
}

// ---------------------------------------------------------------------------
// AC-R16: (0,50,10) → (160, 69)
//   x_screen = 160 +  0/10 = 160.0
//   y_screen =  64 + 50/10 =  64 + 5 = 69.0
// ---------------------------------------------------------------------------
TEST_CASE("AC-R16: (0,50,10) projects to (160, 69) - y-positive offset", "[render][projection]") {
    // Given: camera at origin, world_point = (0, 50, 10)
    // When:  project() is called
    // Then:  x_screen = 160 + 0/10 = 160.0,  y_screen = 64 + 50/10 = 69.0
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, 50.0f, 10.0f}, cam);
    REQUIRE(result.has_value());
    CAPTURE(result->x, result->y);
    REQUIRE(result->x == Catch::Approx(160.0f).margin(kProjEps));
    REQUIRE(result->y == Catch::Approx(69.0f).margin(kProjEps));
}

// ---------------------------------------------------------------------------
// AC-R17: (0,5,10) → (160, 64.5)
//   x_screen = 160 + 0/10 = 160.0
//   y_screen =  64 + 5/10 =  64 + 0.5 = 64.5
// ---------------------------------------------------------------------------
TEST_CASE("AC-R17: (0,5,10) projects to (160, 64.5) - fractional pixel", "[render][projection]") {
    // Given: camera at origin, world_point = (0, 5, 10)
    // When:  project() is called
    // Then:  x_screen = 160 + 0/10 = 160.0,  y_screen = 64 + 5/10 = 64.5
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, 5.0f, 10.0f}, cam);
    REQUIRE(result.has_value());
    CAPTURE(result->x, result->y);
    REQUIRE(result->x == Catch::Approx(160.0f).margin(kProjEps));
    REQUIRE(result->y == Catch::Approx(64.5f).margin(kProjEps));
}

// ---------------------------------------------------------------------------
// AC-R18: (20,-30,10) → (162, 61)
//   x_screen = 160 +  20/10 = 160 +  2 = 162.0
//   y_screen =  64 + -30/10 =  64 + -3 =  61.0
// ---------------------------------------------------------------------------
TEST_CASE("AC-R18: (20,-30,10) projects to (162, 61) - negative y component", "[render][projection]") {
    // Given: camera at origin, world_point = (20, -30, 10)
    // When:  project() is called
    // Then:  x_screen = 160 + 20/10 = 162.0,  y_screen = 64 + (-30)/10 = 61.0
    const render::Camera cam{};
    const auto result = render::project(Vec3{20.0f, -30.0f, 10.0f}, cam);
    REQUIRE(result.has_value());
    CAPTURE(result->x, result->y);
    REQUIRE(result->x == Catch::Approx(162.0f).margin(kProjEps));
    REQUIRE(result->y == Catch::Approx(61.0f).margin(kProjEps));
}

// ===========================================================================
// GROUP 5: Y-FLIP CRITICAL GUARD (AC-R20..R22)
// ===========================================================================
//
// ============================================================================
//  Y-FLIP CRITICAL GUARD - bug-class fence
// ============================================================================
//
//  Pass 3's central design decision (D-Yflip in docs/plans/pass-3-projection.md)
//  is that projection applies NO y-negation.  World Y-DOWN ≡ raylib 2D screen
//  Y-DOWN ≡ Mode 13 screen Y-DOWN.  The original ARM uses y_screen = 64 + y/z
//  (no negation).
//
//  These three ACs catch the bug class that killed prior iterations: someone
//  bolting on `-y/z` or `kScreenCenterY - y/z` "for safety".  If any of these
//  fail, STOP and re-read ADR-0006 before changing the test - the test is
//  almost certainly right and the implementation is almost certainly wrong.
// ============================================================================

// ---------------------------------------------------------------------------
// AC-R20: (0,+10,10) - positive y in Y-DOWN world = below midline
//   y_screen = 64 + 10/10 = 65.0  →  MUST be > kScreenCenterY (64)
//   A y-negation bug produces 64 + (-10/10) = 63.0 < 64, which FAILS this test.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R20: positive world-y projects BELOW screen centre (y_screen > kScreenCenterY)", "[render][projection]") {
    // Given: camera at origin, world_point = (0, +10, 10)
    // When:  project() is called
    // Then:  y_screen = 64 + 10/10 = 65.0 > kScreenCenterY (64)
    //        A y-negation would produce 63.0 < 64 - this test catches that bug.
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, 10.0f, 10.0f}, cam);
    REQUIRE(result.has_value());
    CAPTURE(result->y, render::kScreenCenterY);
    // Belt-and-suspenders: also check the exact pixel
    REQUIRE(result->y == Catch::Approx(65.0f).margin(kProjEps));
    // The critical inequality:
    REQUIRE(result->y > render::kScreenCenterY);
}

// ---------------------------------------------------------------------------
// AC-R21: (0,-10,10) - negative y in Y-DOWN world = above midline
//   y_screen = 64 + (-10)/10 = 63.0  →  MUST be < kScreenCenterY (64)
//   A y-negation bug produces 64 + (10/10) = 65.0 > 64, which FAILS this test.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R21: negative world-y projects ABOVE screen centre (y_screen < kScreenCenterY)", "[render][projection]") {
    // Given: camera at origin, world_point = (0, -10, 10)
    // When:  project() is called
    // Then:  y_screen = 64 + (-10)/10 = 63.0 < kScreenCenterY (64)
    //        A y-negation would produce 65.0 > 64 - this test catches that bug.
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, -10.0f, 10.0f}, cam);
    REQUIRE(result.has_value());
    CAPTURE(result->y, render::kScreenCenterY);
    // Belt-and-suspenders: also check the exact pixel
    REQUIRE(result->y == Catch::Approx(63.0f).margin(kProjEps));
    // The critical inequality:
    REQUIRE(result->y < render::kScreenCenterY);
}

// ---------------------------------------------------------------------------
// AC-R22: (0,+10,10) and (0,-10,10) - deviations from centre are equal
//         in magnitude and opposite in sign.
//   dev_pos = (64 + 10/10) - 64 = +1.0
//   dev_neg = (64 + -10/10) - 64 = -1.0
//   |dev_pos| == |dev_neg|  AND  dev_pos == -dev_neg
// ---------------------------------------------------------------------------
TEST_CASE("AC-R22: y-deviations for +y and -y world points are equal magnitude and opposite sign", "[render][projection]") {
    // Given: camera at origin
    // When:  (0,+10,10) and (0,-10,10) are projected
    // Then:  (y_pos - kScreenCenterY) == -(y_neg - kScreenCenterY)
    //        i.e. deviations are equal magnitude, opposite sign (no spurious offset)
    const render::Camera cam{};
    const auto r_pos = render::project(Vec3{0.0f,  10.0f, 10.0f}, cam);
    const auto r_neg = render::project(Vec3{0.0f, -10.0f, 10.0f}, cam);
    REQUIRE(r_pos.has_value());
    REQUIRE(r_neg.has_value());

    const float dev_pos = r_pos->y - render::kScreenCenterY;  // expected +1.0
    const float dev_neg = r_neg->y - render::kScreenCenterY;  // expected -1.0
    CAPTURE(r_pos->y, r_neg->y, dev_pos, dev_neg);

    // Magnitudes equal:
    REQUIRE(std::abs(dev_pos) == Catch::Approx(std::abs(dev_neg)).margin(kProjEps));
    // Signs opposite:
    REQUIRE(dev_pos == Catch::Approx(-dev_neg).margin(kProjEps));
}

// ===========================================================================
// GROUP 6: Behind/at/near-camera culling (AC-R30..R35)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R30: (0,0,-1) - behind camera → nullopt
// ---------------------------------------------------------------------------
TEST_CASE("AC-R30: point at z=-1 (behind camera) is culled", "[render][projection]") {
    // Given: camera at origin, world_point = (0, 0, -1)
    // When:  project() is called
    // Then:  z_c = -1 <= 0 <= kNearCullZ → nullopt
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, 0.0f, -1.0f}, cam);
    REQUIRE_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// AC-R31: (0,0,-0.0001f) - just behind camera → nullopt
// ---------------------------------------------------------------------------
TEST_CASE("AC-R31: point at z=-0.0001 (just behind camera) is culled", "[render][projection]") {
    // Given: camera at origin, world_point = (0, 0, -0.0001)
    // When:  project() is called
    // Then:  z_c = -0.0001 < 0 → nullopt
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, 0.0f, -0.0001f}, cam);
    REQUIRE_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// AC-R32: (0,0,0) - exactly at camera plane → nullopt
// ---------------------------------------------------------------------------
TEST_CASE("AC-R32: point at z=0 (at camera plane) is culled", "[render][projection]") {
    // Given: camera at origin, world_point = (0, 0, 0)
    // When:  project() is called
    // Then:  z_c = 0 <= kNearCullZ → nullopt (avoids divide-by-zero)
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, 0.0f, 0.0f}, cam);
    REQUIRE_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// AC-R33: (0,0,kNearCullZ) - exactly at near boundary → nullopt  (<= rule)
//   The near cull uses z_c <= kNearCullZ: the boundary itself is culled.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R33: point at exactly kNearCullZ is culled (boundary <= rule)", "[render][projection]") {
    // Given: camera at origin, world_point z = kNearCullZ exactly
    // When:  project() is called
    // Then:  z_c == kNearCullZ and the <= rule culls it → nullopt
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, 0.0f, render::kNearCullZ}, cam);
    REQUIRE_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// AC-R34: (0,0,kNearCullZ + 1e-4f) - just past near boundary → Some
// ---------------------------------------------------------------------------
TEST_CASE("AC-R34: point just past kNearCullZ is NOT culled and returns Some", "[render][projection]") {
    // Given: camera at origin, world_point z = kNearCullZ + kBoundaryEps
    // When:  project() is called
    // Then:  z_c > kNearCullZ → not culled → has_value()
    const render::Camera cam{};
    const float z = render::kNearCullZ + kBoundaryEps;
    const auto result = render::project(Vec3{0.0f, 0.0f, z}, cam);
    REQUIRE(result.has_value());
}

// ---------------------------------------------------------------------------
// AC-R35: (100,200,kNearCullZ+1e-4f) - just past near boundary, off-axis
//         → finite result far from screen centre
// ---------------------------------------------------------------------------
TEST_CASE("AC-R35: off-axis point just past kNearCullZ returns finite far-from-centre pixel", "[render][projection]") {
    // Given: camera at origin, world_point = (100, 200, kNearCullZ + kBoundaryEps)
    // When:  project() is called
    // Then:  result has_value(); x and y are finite (not NaN/Inf)
    //        x_screen = 160 + 100/z_c >> 160 (large divisor is tiny)
    //        y_screen =  64 + 200/z_c >> 64
    const render::Camera cam{};
    const float z = render::kNearCullZ + kBoundaryEps;
    const auto result = render::project(Vec3{100.0f, 200.0f, z}, cam);
    REQUIRE(result.has_value());
    CAPTURE(result->x, result->y, z);
    REQUIRE(std::isfinite(result->x));
    REQUIRE(std::isfinite(result->y));
    // x and y must be far from centre (large offset / tiny z)
    REQUIRE(std::abs(result->x - render::kScreenCenterX) > 1.0f);
    REQUIRE(std::abs(result->y - render::kScreenCenterY) > 1.0f);
}

// ===========================================================================
// GROUP 7: Too-far culling (AC-R40..R42)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R40: (0,0,2e6f) - beyond kFarCullZ (1e6) → nullopt
// ---------------------------------------------------------------------------
TEST_CASE("AC-R40: point at z=2e6 (beyond kFarCullZ) is culled", "[render][projection]") {
    // Given: camera at origin, world_point z = 2e6 (> kFarCullZ = 1e6)
    // When:  project() is called
    // Then:  z_c >= kFarCullZ → nullopt
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, 0.0f, 2.0e6f}, cam);
    REQUIRE_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// AC-R41: (0,0,kFarCullZ) - exactly at far boundary → nullopt  (>= rule)
//   The far cull uses z_c >= kFarCullZ: the boundary itself is culled.
// ---------------------------------------------------------------------------
TEST_CASE("AC-R41: point at exactly kFarCullZ is culled (boundary >= rule)", "[render][projection]") {
    // Given: camera at origin, world_point z = kFarCullZ exactly
    // When:  project() is called
    // Then:  z_c == kFarCullZ and the >= rule culls it → nullopt
    const render::Camera cam{};
    const auto result = render::project(Vec3{0.0f, 0.0f, render::kFarCullZ}, cam);
    REQUIRE_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// AC-R42: (0,0,kFarCullZ - 1.0f) - just inside far boundary → Some
// ---------------------------------------------------------------------------
TEST_CASE("AC-R42: point just inside kFarCullZ is NOT culled and returns Some", "[render][projection]") {
    // Given: camera at origin, world_point z = kFarCullZ - 1.0f
    // When:  project() is called
    // Then:  z_c < kFarCullZ → not culled → has_value()
    const render::Camera cam{};
    const float z = render::kFarCullZ - 1.0f;
    const auto result = render::project(Vec3{0.0f, 0.0f, z}, cam);
    REQUIRE(result.has_value());
}

// ===========================================================================
// GROUP 8: Camera offset (AC-R50..R52)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R50: Camera at (0,0,-5), point (0,0,0)
//   z_c = z_world - z_cam = 0 - (-5) = +5
//   x_screen = 160 + 0/5 = 160,  y_screen = 64 + 0/5 = 64
// ---------------------------------------------------------------------------
TEST_CASE("AC-R50: camera at (0,0,-5) with point at origin gives z_cam=5 → (160, 64)", "[render][projection]") {
    // Given: camera at (0, 0, -5), world_point = (0, 0, 0)
    // When:  project() is called
    // Then:  z_c = 0 - (-5) = 5; x_screen = 160 + 0/5 = 160; y_screen = 64
    const render::Camera cam{Vec3{0.0f, 0.0f, -5.0f}};
    const auto result = render::project(Vec3{0.0f, 0.0f, 0.0f}, cam);
    REQUIRE(result.has_value());
    CAPTURE(result->x, result->y);
    REQUIRE(result->x == Catch::Approx(160.0f).margin(kProjEps));
    REQUIRE(result->y == Catch::Approx(64.0f).margin(kProjEps));
}

// ---------------------------------------------------------------------------
// AC-R51: Camera at (10,20,-5), point (10,20,0)
//   x_c = 10 - 10 = 0,  y_c = 20 - 20 = 0,  z_c = 0 - (-5) = 5
//   x_screen = 160 + 0/5 = 160,  y_screen = 64 + 0/5 = 64
// ---------------------------------------------------------------------------
TEST_CASE("AC-R51: camera at (10,20,-5) with matching xy point gives screen centre (160, 64)", "[render][projection]") {
    // Given: camera at (10, 20, -5), world_point = (10, 20, 0)
    // When:  project() is called
    // Then:  x_c=0, y_c=0, z_c=5 → screen centre
    const render::Camera cam{Vec3{10.0f, 20.0f, -5.0f}};
    const auto result = render::project(Vec3{10.0f, 20.0f, 0.0f}, cam);
    REQUIRE(result.has_value());
    CAPTURE(result->x, result->y);
    REQUIRE(result->x == Catch::Approx(160.0f).margin(kProjEps));
    REQUIRE(result->y == Catch::Approx(64.0f).margin(kProjEps));
}

// ---------------------------------------------------------------------------
// AC-R52: Camera at (0,-10,0), point (0,0,10)
//   x_c = 0 - 0 = 0,  y_c = 0 - (-10) = +10,  z_c = 10 - 0 = 10
//   x_screen = 160 + 0/10 = 160,  y_screen = 64 + 10/10 = 65
// ---------------------------------------------------------------------------
TEST_CASE("AC-R52: camera at (0,-10,0) offsets y_cam; point (0,0,10) → y_screen=65", "[render][projection]") {
    // Given: camera at (0, -10, 0), world_point = (0, 0, 10)
    // When:  project() is called
    // Then:  y_c = 0 - (-10) = 10; z_c = 10; y_screen = 64 + 10/10 = 65
    const render::Camera cam{Vec3{0.0f, -10.0f, 0.0f}};
    const auto result = render::project(Vec3{0.0f, 0.0f, 10.0f}, cam);
    REQUIRE(result.has_value());
    CAPTURE(result->x, result->y);
    REQUIRE(result->x == Catch::Approx(160.0f).margin(kProjEps));
    REQUIRE(result->y == Catch::Approx(65.0f).margin(kProjEps));
}

// ===========================================================================
// GROUP 9: Aspect-ratio invariant (AC-R60..R62)
// ===========================================================================
//
// D-AspectRatio: projection uses a single divisor z for both x and y axes.
// No per-axis correction is applied.  A point at (d, 0, z) and one at
// (0, d, z) must project to the same pixel-distance from screen centre.

// ---------------------------------------------------------------------------
// AC-R60: (10,0,10) and (0,10,10) project to equal pixel-distance from centre
//   dist_x = |160 + 10/10 - 160| = 1.0
//   dist_y = | 64 + 10/10 -  64| = 1.0
//   → dist_x == dist_y  (no aspect correction)
// ---------------------------------------------------------------------------
TEST_CASE("AC-R60: x-only and y-only offsets of same magnitude give equal pixel-distances from centre", "[render][projection]") {
    // Given: camera at origin
    // When:  (10, 0, 10) and (0, 10, 10) are projected
    // Then:  distance from screen-centre is equal in both cases (= 1.0 px)
    const render::Camera cam{};
    const auto rx = render::project(Vec3{10.0f,  0.0f, 10.0f}, cam);
    const auto ry = render::project(Vec3{ 0.0f, 10.0f, 10.0f}, cam);
    REQUIRE(rx.has_value());
    REQUIRE(ry.has_value());

    const float dist_x = std::abs(rx->x - render::kScreenCenterX);
    const float dist_y = std::abs(ry->y - render::kScreenCenterY);
    CAPTURE(dist_x, dist_y, rx->x, ry->y);
    REQUIRE(dist_x == Catch::Approx(dist_y).margin(kProjEps));
    // Also verify the exact expected distance (1.0 px)
    REQUIRE(dist_x == Catch::Approx(1.0f).margin(kProjEps));
}

// ---------------------------------------------------------------------------
// AC-R61: (100,0,10) and (0,100,10) → equal pixel-distance = 10 px
//   dist_x = |160 + 100/10 - 160| = 10.0
//   dist_y = | 64 + 100/10 -  64| = 10.0
// ---------------------------------------------------------------------------
TEST_CASE("AC-R61: x-only and y-only offsets of 100/10 both give 10 px distance from centre", "[render][projection]") {
    // Given: camera at origin
    // When:  (100, 0, 10) and (0, 100, 10) are projected
    // Then:  each is exactly 10 px from screen centre (100/10 = 10)
    const render::Camera cam{};
    const auto rx = render::project(Vec3{100.0f,   0.0f, 10.0f}, cam);
    const auto ry = render::project(Vec3{  0.0f, 100.0f, 10.0f}, cam);
    REQUIRE(rx.has_value());
    REQUIRE(ry.has_value());

    const float dist_x = std::abs(rx->x - render::kScreenCenterX);
    const float dist_y = std::abs(ry->y - render::kScreenCenterY);
    CAPTURE(dist_x, dist_y);
    REQUIRE(dist_x == Catch::Approx(10.0f).margin(kProjEps));
    REQUIRE(dist_y == Catch::Approx(10.0f).margin(kProjEps));
    REQUIRE(dist_x == Catch::Approx(dist_y).margin(kProjEps));
}

// ---------------------------------------------------------------------------
// AC-R62: Per-axis distances are symmetric: positive and negative offsets
//         on the same axis produce equal magnitude pixel deviations
//   (50, 0, 10) → dist = 5;  (-50, 0, 10) → dist = 5
// ---------------------------------------------------------------------------
TEST_CASE("AC-R62: positive and negative world offsets on the same axis give equal pixel-distance", "[render][projection]") {
    // Given: camera at origin
    // When:  (50, 0, 10) and (-50, 0, 10) are projected
    // Then:  |x_screen - 160| is 5 in both cases (symmetric, no bias)
    const render::Camera cam{};
    const auto rp = render::project(Vec3{ 50.0f, 0.0f, 10.0f}, cam);
    const auto rn = render::project(Vec3{-50.0f, 0.0f, 10.0f}, cam);
    REQUIRE(rp.has_value());
    REQUIRE(rn.has_value());

    const float dist_p = std::abs(rp->x - render::kScreenCenterX);
    const float dist_n = std::abs(rn->x - render::kScreenCenterX);
    CAPTURE(dist_p, dist_n, rp->x, rn->x);
    REQUIRE(dist_p == Catch::Approx(5.0f).margin(kProjEps));
    REQUIRE(dist_n == Catch::Approx(5.0f).margin(kProjEps));
    REQUIRE(dist_p == Catch::Approx(dist_n).margin(kProjEps));
}

// ===========================================================================
// GROUP 10: Determinism (AC-R70)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-R70: project((3,7,11), origin-camera) called 1000 times is bit-identical
// ---------------------------------------------------------------------------
TEST_CASE("AC-R70: project() called 1000 times with same inputs returns bit-identical results", "[render][projection]") {
    // Given: camera at origin, world_point = (3, 7, 11) - arbitrary non-trivial input
    // When:  project() is called 1000 times in a tight loop
    // Then:  every result is bit-identical to the first (pure function, no hidden state)
    const render::Camera cam{};
    const Vec3 pt{3.0f, 7.0f, 11.0f};
    const auto reference = render::project(pt, cam);
    REQUIRE(reference.has_value());

    for (int i = 0; i < 1000; ++i) {
        const auto result = render::project(pt, cam);
        REQUIRE(result.has_value());
        CAPTURE(i, result->x, result->y, reference->x, reference->y);
        // Bit-identical: exact equality, NOT Approx
        REQUIRE(result->x == reference->x);
        REQUIRE(result->y == reference->y);
    }
}

// ===========================================================================
// GROUP 11: Architecture hygiene (AC-R81..R82)
// ===========================================================================
//
// AC-R80 (no-raylib static_assert) is at the top of this file, immediately
// after the #include "render/projection.hpp".
//
// AC-R81: Build with -DCLAUDE_LANDER_BUILD_GAME=OFF succeeds; tests link
//         claude_lander_render without raylib.
//   → Verified by the build itself; no runtime TEST_CASE needed.
//   → The BUILD_GAME=OFF CMake invocation is the test.
//
// AC-R82: claude_lander_render link list = claude_lander_core +
//         claude_lander_warnings only.
//   → Verified by the CMakeLists.txt; no runtime TEST_CASE needed.
//   → Implementer MUST NOT add raylib to claude_lander_render's link list.

TEST_CASE("AC-R81: projection header and library compile and link without raylib (BUILD_GAME=OFF)", "[render][projection]") {
    // Given: this test file was compiled with BUILD_GAME=OFF (no raylib on path)
    // When:  it reaches this TEST_CASE at runtime
    // Then:  it ran - which means projection.hpp + vec2.hpp compiled and linked
    //        without raylib, satisfying AC-R81.
    //        If compilation fails, the red-state error messages document the issue.
    SUCCEED("compilation and linkage without raylib succeeded - AC-R81 satisfied");
}

TEST_CASE("AC-R82: kLogicalScreenW and kLogicalScreenH constants are exposed with correct values", "[render][projection]") {
    // Given: render::kLogicalScreenW and render::kLogicalScreenH are declared
    //        in projection.hpp (per planner spec §5)
    // When:  their values are read
    // Then:  kLogicalScreenW == 320 and kLogicalScreenH == 256 (Mode 13)
    REQUIRE(render::kLogicalScreenW == 320);
    REQUIRE(render::kLogicalScreenH == 256);
    // Also verify the screen-centre and cull constants are accessible and sane
    REQUIRE(render::kScreenCenterX == Catch::Approx(160.0f).margin(kProjEps));
    REQUIRE(render::kScreenCenterY == Catch::Approx(64.0f).margin(kProjEps));
    REQUIRE(render::kNearCullZ > 0.0f);
    REQUIRE(render::kFarCullZ > render::kNearCullZ);
}

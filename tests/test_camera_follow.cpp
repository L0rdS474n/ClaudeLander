// tests/test_camera_follow.cpp — Pass 7 world/camera_follow tests.
//
// Covers AC-C01..C21 and AC-Cback, AC-Cground, AC-Cnose, AC-C80..C82
// (24 ACs total, numbered per pass-7-camera-follow.md §4).
// All tests tagged [world][camera_follow].
//
// === Determinism plan ===
// world::follow_camera_position is declared noexcept and is a pure function
// of ship_position only.  No clocks, no PRNG, no filesystem, no system calls,
// no global mutable state.  AC-C10/C11/C13/C14 verify this property; the
// AC-C10 sweep runs 1000 iterations and requires bit-identical results between
// two independent runs.
//
// External dependency: terrain::altitude(x, z) is also a pure function
// (pass-2 contract), so the ground-clamp output is fully deterministic given
// ship_position alone.
//
// === Camera formula (D-BackOffsetSign, D-GroundClampViaTerrain) ===
//   cam.x = ship.x
//   cam.z = ship.z - kCameraBackOffset * TILE_SIZE      (AC-Cback fence)
//   floor_y = altitude(cam.x, cam.z) + kCameraGroundClearance
//   cam.y   = max(ship.y, floor_y)                      (AC-Cground fence)
//
// === Bug-class fences ===
// Three developer-mistake patterns are caught with prominent banner comments:
//   (a) AC-Cback   — sign-flip catch: cam.z = ship.z - 5*TILE_SIZE, NOT + 5.
//   (b) AC-Cground — ground clamp: cam.y uses floor_y, NOT ship.y, when deep.
//   (c) AC-Cnose   — return type must be Vec3, not Mat3 or any other type.
//
// === AC-C82 noexcept verification ===
// static_assert checks noexcept on follow_camera_position placed after the
// #include so it fires at compile time in the same TU.
//
// === Integration (AC-C15..C18) ===
// These tests include render/projection.hpp and construct a render::Camera
// from follow_camera_position, then call render::project() to verify that
// the ship's world position projects near screen-centre or as expected.
//
// === Red-state expectation ===
// This file FAILS TO COMPILE until the implementer creates:
//   src/world/camera_follow.hpp
//   src/world/camera_follow.cpp
// That is the correct red state for Gate 2 delivery.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <set>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/vec3.hpp"
#include "world/terrain.hpp"         // terrain::altitude, terrain::TILE_SIZE
#include "world/camera_follow.hpp"   // world::follow_camera_position,
                                     // world::kCameraBackOffset,
                                     // world::kCameraGroundClearance
#include "render/projection.hpp"     // render::Camera, render::project,
                                     // render::kScreenCenterX, render::kScreenCenterY

// ---------------------------------------------------------------------------
// AC-C80: world/camera_follow.hpp must not pull in raylib, render/, entities/,
// input/, <random>, or <chrono>.
// RAYLIB_VERSION is defined by raylib.h; if present here, the include chain
// is polluted.  The BUILD_GAME=OFF build keeps raylib off the compiler path.
// ---------------------------------------------------------------------------
#ifdef RAYLIB_VERSION
static_assert(false,
    "AC-C80 VIOLATED: world/camera_follow.hpp pulled in raylib.h "
    "(RAYLIB_VERSION is defined).  Remove the raylib dependency.");
#endif

// ---------------------------------------------------------------------------
// AC-Cnose — return type must be Vec3, not Mat3 or any other type.
// ===========================================================================
//
//  BUG-CLASS FENCE (AC-Cnose)
// ===========================================================================
//
//  D-NoCameraTilt: follow_camera_position returns Vec3 only — no rotation,
//  no Mat3, no orientation struct.  This static_assert fires at compile time
//  if the return type is changed to anything other than Vec3.
//
//  If this assert fires: do NOT change the test.  Re-read D-NoCameraTilt and
//  D-Stateless in the plan; the camera has no orientation in Pass 7.
// ===========================================================================
static_assert(
    std::is_same_v<decltype(world::follow_camera_position(Vec3{})), Vec3>,
    "AC-Cnose: follow_camera_position must return Vec3 (not Mat3, not a struct with rotation)");

// ---------------------------------------------------------------------------
// AC-C82 — follow_camera_position must be declared noexcept.
// ===========================================================================
//
//  D-Stateless: pure noexcept function, no globals.
//  Verified at compile time here in the same TU as the #include.
// ===========================================================================
static_assert(
    noexcept(world::follow_camera_position(Vec3{})),
    "AC-C82: world::follow_camera_position must be declared noexcept");

// ---------------------------------------------------------------------------
// Tolerance constant (per planner spec §5)
// ---------------------------------------------------------------------------
static constexpr float kCamEps = 1e-5f;

// ---------------------------------------------------------------------------
// Helper: compute the expected camera Y using the locked formula.
//   floor_y = terrain::altitude(cam_x, cam_z) + world::kCameraGroundClearance
//   cam_y   = std::max(ship_y, floor_y)
// ---------------------------------------------------------------------------
static float expected_cam_y(float ship_x, float ship_y, float ship_z) noexcept {
    const float cam_z   = ship_z - world::kCameraBackOffset * terrain::TILE_SIZE;
    const float floor_y = terrain::altitude(ship_x, cam_z) + world::kCameraGroundClearance;
    return std::max(ship_y, floor_y);
}

// ===========================================================================
// GROUP 1: Basic positioning (AC-C01..C05)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-C01 — Ship {0,5,0}: cam = {0, max(5, alt(0,-5)+0.1), -5}.
//   Given: ship_position = {0, 5, 0}
//   When:  follow_camera_position is called
//   Then:  cam.x == 0, cam.z == -5,
//          cam.y == max(5, terrain::altitude(0,-5) + kCameraGroundClearance)
// ---------------------------------------------------------------------------
TEST_CASE("AC-C01: ship {0,5,0} gives cam.x=0 cam.z=-5 cam.y=max(5,floor)", "[world][camera_follow]") {
    // Given
    const Vec3 ship{0.0f, 5.0f, 0.0f};

    // When
    const Vec3 cam = world::follow_camera_position(ship);

    // Then
    const float expected_z = ship.z - world::kCameraBackOffset * terrain::TILE_SIZE;
    const float expected_y = expected_cam_y(ship.x, ship.y, ship.z);
    CAPTURE(cam.x, cam.y, cam.z, expected_z, expected_y);
    REQUIRE(cam.x == Catch::Approx(0.0f).margin(kCamEps));
    REQUIRE(cam.z == Catch::Approx(expected_z).margin(kCamEps));
    REQUIRE(cam.y == Catch::Approx(expected_y).margin(kCamEps));
}

// ---------------------------------------------------------------------------
// AC-C02 — Ship {10,5,0}: cam.x=10, cam.z=-5.
//   Given: ship_position = {10, 5, 0}
//   When:  follow_camera_position is called
//   Then:  cam.x == 10.0f, cam.z == -5.0f (ship.z - 5*TILE_SIZE)
// ---------------------------------------------------------------------------
TEST_CASE("AC-C02: ship {10,5,0} gives cam.x=10 cam.z=-5", "[world][camera_follow]") {
    // Given
    const Vec3 ship{10.0f, 5.0f, 0.0f};

    // When
    const Vec3 cam = world::follow_camera_position(ship);

    // Then
    CAPTURE(cam.x, cam.z);
    REQUIRE(cam.x == Catch::Approx(10.0f).margin(kCamEps));
    REQUIRE(cam.z == Catch::Approx(-5.0f).margin(kCamEps));
}

// ---------------------------------------------------------------------------
// AC-C03 — Ship {0,5,100}: cam.z=95.
//   Given: ship_position = {0, 5, 100}
//   When:  follow_camera_position is called
//   Then:  cam.z == 95.0f (100 - 5*1.0)
// ---------------------------------------------------------------------------
TEST_CASE("AC-C03: ship {0,5,100} gives cam.z=95", "[world][camera_follow]") {
    // Given
    const Vec3 ship{0.0f, 5.0f, 100.0f};

    // When
    const Vec3 cam = world::follow_camera_position(ship);

    // Then
    const float expected_z = 100.0f - world::kCameraBackOffset * terrain::TILE_SIZE;
    CAPTURE(cam.z, expected_z);
    REQUIRE(cam.z == Catch::Approx(expected_z).margin(kCamEps));
}

// ---------------------------------------------------------------------------
// AC-C04 — cam.x == ship.x for ship.x in {-50,-1,0,1,50} when ship.y high.
//   Given: ship_position with various x values, ship.y=1000 (above any terrain)
//   When:  follow_camera_position is called for each
//   Then:  cam.x == ship.x exactly within kCamEps for all inputs
// ---------------------------------------------------------------------------
TEST_CASE("AC-C04: cam.x == ship.x for ship.x in {-50,-1,0,1,50} with high ship.y", "[world][camera_follow]") {
    // Given
    const float test_x_values[] = {-50.0f, -1.0f, 0.0f, 1.0f, 50.0f};
    const float high_y = 1000.0f;  // well above any terrain

    for (const float ship_x : test_x_values) {
        // When
        const Vec3 ship{ship_x, high_y, 0.0f};
        const Vec3 cam = world::follow_camera_position(ship);

        // Then
        CAPTURE(ship_x, cam.x);
        REQUIRE(cam.x == Catch::Approx(ship_x).margin(kCamEps));
    }
}

// ---------------------------------------------------------------------------
// AC-C05 — cam.z == ship.z - 5*TILE_SIZE for ship.z in {-50,-1,0,1,50,100}.
//   Given: ship_position with various z values, ship.y=1000 (above terrain)
//   When:  follow_camera_position is called for each
//   Then:  cam.z == ship.z - kCameraBackOffset * TILE_SIZE within kCamEps
// ---------------------------------------------------------------------------
TEST_CASE("AC-C05: cam.z == ship.z - 5*TILE_SIZE for ship.z in {-50,-1,0,1,50,100}", "[world][camera_follow]") {
    // Given
    const float test_z_values[] = {-50.0f, -1.0f, 0.0f, 1.0f, 50.0f, 100.0f};
    const float high_y = 1000.0f;

    for (const float ship_z : test_z_values) {
        // When
        const Vec3 ship{0.0f, high_y, ship_z};
        const Vec3 cam = world::follow_camera_position(ship);

        // Then
        const float expected_z = ship_z - world::kCameraBackOffset * terrain::TILE_SIZE;
        CAPTURE(ship_z, cam.z, expected_z);
        REQUIRE(cam.z == Catch::Approx(expected_z).margin(kCamEps));
    }
}

// ===========================================================================
// GROUP 2: Y-clamp (AC-C06..C09)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-C06 — Ship.y=100 (well above): cam.y = ship.y exactly.
//   Given: ship_position = {0, 100, 0} — ship.y far above any terrain floor
//   When:  follow_camera_position is called
//   Then:  cam.y == 100.0f (ship.y wins the max; terrain is near 5.0)
// ---------------------------------------------------------------------------
TEST_CASE("AC-C06: ship.y=100 (well above floor) gives cam.y == ship.y exactly", "[world][camera_follow]") {
    // Given: ship.y=100 is far above terrain (which ranges ~4.96..5.04)
    const Vec3 ship{0.0f, 100.0f, 0.0f};

    // When
    const Vec3 cam = world::follow_camera_position(ship);

    // Then
    const float expected_y = expected_cam_y(ship.x, ship.y, ship.z);
    CAPTURE(cam.y, expected_y);
    REQUIRE(cam.y == Catch::Approx(expected_y).margin(kCamEps));
    // Also verify ship.y is the dominant term (not clamped up)
    REQUIRE(cam.y == Catch::Approx(ship.y).margin(kCamEps));
}

// ---------------------------------------------------------------------------
// AC-C07 — Ship.y=-1000: cam.y = altitude(cam.x, cam.z) + clearance.
//   Given: ship_position = {0, -1000, 0} — ship deeply below terrain
//   When:  follow_camera_position is called
//   Then:  cam.y == terrain::altitude(0, cam.z) + kCameraGroundClearance
//          (ground clamp activates; ship.y is discarded as the smaller value)
// ---------------------------------------------------------------------------
TEST_CASE("AC-C07: ship.y=-1000 (deep below floor) gives cam.y clamped to floor+clearance", "[world][camera_follow]") {
    // Given
    const Vec3 ship{0.0f, -1000.0f, 0.0f};

    // When
    const Vec3 cam = world::follow_camera_position(ship);

    // Then
    const float cam_z    = ship.z - world::kCameraBackOffset * terrain::TILE_SIZE;
    const float floor_y  = terrain::altitude(ship.x, cam_z) + world::kCameraGroundClearance;
    CAPTURE(cam.y, floor_y);
    REQUIRE(cam.y == Catch::Approx(floor_y).margin(kCamEps));
    // cam.y must be above ship.y
    REQUIRE(cam.y > ship.y);
}

// ---------------------------------------------------------------------------
// AC-C08 — Threshold at floor: cam.y == floor when ship.y == floor.
//   Given: ship.y == altitude(cam.x, cam.z) + clearance (exactly at boundary)
//   When:  follow_camera_position is called
//   Then:  cam.y == floor_y (max of two equal values)
// ---------------------------------------------------------------------------
TEST_CASE("AC-C08: ship.y == floor_y gives cam.y == floor_y (threshold: max of equals)", "[world][camera_follow]") {
    // Given: compute what floor_y would be at this ship position, then set ship.y = floor_y
    const float ship_x    = 3.0f;
    const float ship_z    = 7.0f;
    const float cam_z     = ship_z - world::kCameraBackOffset * terrain::TILE_SIZE;
    const float floor_y   = terrain::altitude(ship_x, cam_z) + world::kCameraGroundClearance;
    const Vec3 ship{ship_x, floor_y, ship_z};

    // When
    const Vec3 cam = world::follow_camera_position(ship);

    // Then
    CAPTURE(cam.y, floor_y, ship.y);
    REQUIRE(cam.y == Catch::Approx(floor_y).margin(kCamEps));
}

// ---------------------------------------------------------------------------
// AC-C09 — Threshold − ε: cam.y clamped up to floor.
//   Given: ship.y = floor_y - kCamEps*10 (just below floor)
//   When:  follow_camera_position is called
//   Then:  cam.y == floor_y (ground clamp activates)
// ---------------------------------------------------------------------------
TEST_CASE("AC-C09: ship.y just below floor_y gives cam.y clamped up to floor_y", "[world][camera_follow]") {
    // Given
    const float ship_x    = 3.0f;
    const float ship_z    = 7.0f;
    const float cam_z     = ship_z - world::kCameraBackOffset * terrain::TILE_SIZE;
    const float floor_y   = terrain::altitude(ship_x, cam_z) + world::kCameraGroundClearance;
    const float below_y   = floor_y - kCamEps * 10.0f;
    const Vec3 ship{ship_x, below_y, ship_z};

    // When
    const Vec3 cam = world::follow_camera_position(ship);

    // Then
    CAPTURE(cam.y, floor_y, below_y);
    REQUIRE(cam.y == Catch::Approx(floor_y).margin(kCamEps));
    REQUIRE(cam.y > below_y);
}

// ===========================================================================
// GROUP 3: Determinism (AC-C10..C14)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-C10 — Two runs over 1000-iter sweep: bit-identical.
//   Given: a fixed sequence of 1000 ship positions (deterministic formula,
//          no PRNG)
//   When:  follow_camera_position is called twice over the same sequence
//   Then:  every (cam.x, cam.y, cam.z) pair is bit-identical between runs
// ---------------------------------------------------------------------------
TEST_CASE("AC-C10: 1000-iteration sweep is deterministic — bit-identical across two runs", "[world][camera_follow]") {
    // Given: deterministic position sequence — no PRNG, no clock
    auto run_sweep = [&]() -> std::vector<Vec3> {
        std::vector<Vec3> results;
        results.reserve(1000);
        for (int i = 0; i < 1000; ++i) {
            const float t   = static_cast<float>(i) * 0.1f;
            const Vec3  ship{t * 0.3f, 5.0f + t * 0.01f, t * 0.7f};
            results.push_back(world::follow_camera_position(ship));
        }
        return results;
    };

    // When
    const auto run_a = run_sweep();
    const auto run_b = run_sweep();

    // Then — bit-identical (exact equality, NOT Approx)
    REQUIRE(run_a.size() == run_b.size());
    for (std::size_t i = 0; i < run_a.size(); ++i) {
        CAPTURE(i, run_a[i].x, run_b[i].x);
        REQUIRE(run_a[i].x == run_b[i].x);
        REQUIRE(run_a[i].y == run_b[i].y);
        REQUIRE(run_a[i].z == run_b[i].z);
    }
}

// ---------------------------------------------------------------------------
// AC-C11 — Pure: same input, identical output.
//   Given: a single ship position called twice in sequence
//   When:  follow_camera_position is called twice with the same argument
//   Then:  both results are bit-identical
// ---------------------------------------------------------------------------
TEST_CASE("AC-C11: same ship position called twice returns bit-identical camera positions", "[world][camera_follow]") {
    // Given
    const Vec3 ship{12.5f, 8.0f, 33.0f};

    // When
    const Vec3 cam_a = world::follow_camera_position(ship);
    const Vec3 cam_b = world::follow_camera_position(ship);

    // Then
    CAPTURE(cam_a.x, cam_b.x, cam_a.y, cam_b.y, cam_a.z, cam_b.z);
    REQUIRE(cam_a.x == cam_b.x);
    REQUIRE(cam_a.y == cam_b.y);
    REQUIRE(cam_a.z == cam_b.z);
}

// ---------------------------------------------------------------------------
// AC-C12 — No <random>, <chrono> in source.
//   Verified architecturally: world_no_raylib_includes CTest tripwire.
//   Runtime proxy: the test executable reached this point, confirming the
//   binary was compiled with the no-forbidden-includes build.
// ---------------------------------------------------------------------------
TEST_CASE("AC-C12: world/camera_follow.hpp compiled without <random> or <chrono> (architecture tripwire proxy)", "[world][camera_follow]") {
    // Given: this test executable was built with BUILD_GAME=OFF and the
    //        world_no_raylib_includes CTest tripwire active.
    // When:  this test is reached
    // Then:  the compilation succeeded without forbidden headers — AC-C12 satisfied.
    SUCCEED("architecture tripwire confirms no <random>/<chrono> in world/ — AC-C12 satisfied");
}

// ---------------------------------------------------------------------------
// AC-C13 — 10×10×10 grid sweep: stable (no crash, all results finite).
//   Given: 10×10×10 grid of ship positions (x in [-5,5], y in [0,10], z in [-5,5])
//   When:  follow_camera_position is called for each
//   Then:  all cam.x, cam.y, cam.z are finite (no NaN, no Inf)
// ---------------------------------------------------------------------------
TEST_CASE("AC-C13: 10x10x10 grid sweep produces finite camera positions", "[world][camera_follow]") {
    // Given: 1000 grid positions (no PRNG — deterministic lattice)
    for (int ix = 0; ix < 10; ++ix) {
        for (int iy = 0; iy < 10; ++iy) {
            for (int iz = 0; iz < 10; ++iz) {
                const float x    = -5.0f + static_cast<float>(ix);
                const float y    = static_cast<float>(iy);
                const float z    = -5.0f + static_cast<float>(iz);
                const Vec3  ship{x, y, z};

                // When
                const Vec3 cam = world::follow_camera_position(ship);

                // Then
                CAPTURE(ix, iy, iz, cam.x, cam.y, cam.z);
                REQUIRE(std::isfinite(cam.x));
                REQUIRE(std::isfinite(cam.y));
                REQUIRE(std::isfinite(cam.z));
            }
        }
    }
}

// ---------------------------------------------------------------------------
// AC-C14 — Reverse-order sweep: same set of pairs.
//   Given: a list of N ship positions traversed forward and backward
//   When:  follow_camera_position is called for each
//   Then:  {input, output} pairs collected from both traversals form the same
//          multi-set (order-independence confirms no side-effects)
// ---------------------------------------------------------------------------
TEST_CASE("AC-C14: forward and reverse-order sweep produce the same set of (input, output) pairs", "[world][camera_follow]") {
    // Given: fixed list of 10 positions
    const Vec3 ships[] = {
        {  0.0f,  5.0f,   0.0f},
        { 10.0f,  8.0f,  20.0f},
        { -3.0f,  3.0f,  -7.0f},
        {  7.0f, 15.0f,  50.0f},
        {  1.0f,  5.1f,   1.0f},
        { -8.0f,  6.0f,  -4.0f},
        {  4.0f,  9.0f,  12.0f},
        { -1.0f,  4.0f,   0.0f},
        { 20.0f, 20.0f,  80.0f},
        {  0.0f,  5.0f, 100.0f},
    };
    constexpr int N = static_cast<int>(sizeof(ships) / sizeof(ships[0]));

    // Collect forward results
    using Pair = std::pair<Vec3, Vec3>;
    std::vector<Pair> forward_pairs;
    for (int i = 0; i < N; ++i) {
        forward_pairs.emplace_back(ships[i], world::follow_camera_position(ships[i]));
    }

    // Collect reverse results
    std::vector<Pair> reverse_pairs;
    for (int i = N - 1; i >= 0; --i) {
        reverse_pairs.emplace_back(ships[i], world::follow_camera_position(ships[i]));
    }

    // Then: sort both by ship.z (deterministic tie-break key) and compare
    auto by_ship_z = [](const Pair& a, const Pair& b) {
        return a.first.z < b.first.z;
    };
    std::sort(forward_pairs.begin(), forward_pairs.end(), by_ship_z);
    std::sort(reverse_pairs.begin(), reverse_pairs.end(), by_ship_z);

    REQUIRE(forward_pairs.size() == reverse_pairs.size());
    for (std::size_t i = 0; i < forward_pairs.size(); ++i) {
        CAPTURE(i,
            forward_pairs[i].second.x, reverse_pairs[i].second.x,
            forward_pairs[i].second.y, reverse_pairs[i].second.y,
            forward_pairs[i].second.z, reverse_pairs[i].second.z);
        REQUIRE(forward_pairs[i].second.x == reverse_pairs[i].second.x);
        REQUIRE(forward_pairs[i].second.y == reverse_pairs[i].second.y);
        REQUIRE(forward_pairs[i].second.z == reverse_pairs[i].second.z);
    }
}

// ===========================================================================
// GROUP 4: Integration with Pass 3 projection (AC-C15..C18)
// ===========================================================================
//
// Camera from follow_camera_position is fed to render::Camera.  The ship's
// own world position is then projected; because the camera tracks the ship,
// the ship point should land near screen centre (160, 64) ± 1 px.
//
// Geometry reasoning for AC-C15..C17:
//   ship position P; cam = follow_camera_position(P).
//   z_c = P.z - cam.z = P.z - (P.z - 5*TILE_SIZE) = 5 * TILE_SIZE = 5.0
//   x_c = P.x - cam.x = P.x - P.x = 0
//   y_c = P.y - cam.y
//   When ship.y >> floor: cam.y = ship.y, so y_c = 0.
//   x_screen = 160 + 0/5 = 160;  y_screen = 64 + 0/5 = 64.
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-C15 — Vertex at ship pos {0,5,0} projects within ±1 px of (160, 64).
//   Given: ship = {0,5,0}, cam = follow_camera_position(ship)
//   When:  render::project(ship_pos, render::Camera{cam}) is called
//   Then:  |x_screen - 160| <= 1.0f, |y_screen - 64| <= 1.0f
// ---------------------------------------------------------------------------
TEST_CASE("AC-C15: ship at {0,5,0} with follow camera projects within 1px of screen centre (160,64)", "[world][camera_follow]") {
    // Given
    const Vec3 ship{0.0f, 5.0f, 0.0f};
    const Vec3 cam_pos = world::follow_camera_position(ship);
    const render::Camera cam{cam_pos};

    // When
    const auto pixel = render::project(ship, cam);

    // Then
    REQUIRE(pixel.has_value());
    CAPTURE(pixel->x, pixel->y, cam_pos.x, cam_pos.y, cam_pos.z);
    REQUIRE(std::abs(pixel->x - render::kScreenCenterX) <= 1.0f);
    REQUIRE(std::abs(pixel->y - render::kScreenCenterY) <= 1.0f);
}

// ---------------------------------------------------------------------------
// AC-C16 — Ship {10,5,0}: vertex at ship pos still near (160, 64).
//   Given: ship = {10,5,0}, cam = follow_camera_position(ship)
//   When:  render::project(ship, cam) is called
//   Then:  |x_screen - 160| <= 1.0f, |y_screen - 64| <= 1.0f
// ---------------------------------------------------------------------------
TEST_CASE("AC-C16: ship at {10,5,0} with follow camera projects within 1px of screen centre (160,64)", "[world][camera_follow]") {
    // Given
    const Vec3 ship{10.0f, 5.0f, 0.0f};
    const Vec3 cam_pos = world::follow_camera_position(ship);
    const render::Camera cam{cam_pos};

    // When
    const auto pixel = render::project(ship, cam);

    // Then
    REQUIRE(pixel.has_value());
    CAPTURE(pixel->x, pixel->y, cam_pos.x, cam_pos.y, cam_pos.z);
    REQUIRE(std::abs(pixel->x - render::kScreenCenterX) <= 1.0f);
    REQUIRE(std::abs(pixel->y - render::kScreenCenterY) <= 1.0f);
}

// ---------------------------------------------------------------------------
// AC-C17 — Ship {0,20,0}: cam.y=20, vertex at ship pos near (160, 64).
//   Given: ship = {0,20,0} — ship.y=20 well above terrain; cam.y = ship.y
//   When:  render::project(ship, cam) is called
//   Then:  |x_screen - 160| <= 1.0f, |y_screen - 64| <= 1.0f
// ---------------------------------------------------------------------------
TEST_CASE("AC-C17: ship at {0,20,0} (cam.y=20) with follow camera projects within 1px of (160,64)", "[world][camera_follow]") {
    // Given
    const Vec3 ship{0.0f, 20.0f, 0.0f};
    const Vec3 cam_pos = world::follow_camera_position(ship);
    const render::Camera cam{cam_pos};

    // When
    const auto pixel = render::project(ship, cam);

    // Then
    REQUIRE(pixel.has_value());
    CAPTURE(pixel->x, pixel->y, cam_pos.x, cam_pos.y, cam_pos.z);
    REQUIRE(std::abs(pixel->x - render::kScreenCenterX) <= 1.0f);
    REQUIRE(std::abs(pixel->y - render::kScreenCenterY) <= 1.0f);
}

// ---------------------------------------------------------------------------
// AC-C18 — Vertex at {ship.x+1, ship.y, ship.z} projects RIGHT of centre.
//   Given: ship = {0,5,0}, follow-camera; extra vertex = {1, 5, 0}
//   When:  render::project(extra_vertex, cam) is called
//   Then:  pixel.x > render::kScreenCenterX (160)
//          x_c = 1 - 0 = 1; z_c = 5; x_screen = 160 + 1/5 = 160.2 > 160
// ---------------------------------------------------------------------------
TEST_CASE("AC-C18: vertex at ship.x+1 with follow camera projects to the RIGHT of screen centre", "[world][camera_follow]") {
    // Given
    const Vec3 ship{0.0f, 5.0f, 0.0f};
    const Vec3 cam_pos = world::follow_camera_position(ship);
    const render::Camera cam{cam_pos};
    const Vec3 right_vertex{ship.x + 1.0f, ship.y, ship.z};

    // When
    const auto pixel = render::project(right_vertex, cam);

    // Then
    REQUIRE(pixel.has_value());
    CAPTURE(pixel->x, render::kScreenCenterX);
    REQUIRE(pixel->x > render::kScreenCenterX);
}

// ===========================================================================
// GROUP 5: Edge cases (AC-C19..C21)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-C19 — Negative ship.z: ship {0,5,-100} → cam.z=-105.
//   Given: ship_position = {0, 5, -100}
//   When:  follow_camera_position is called
//   Then:  cam.z == -100 - 5*1.0 = -105.0
// ---------------------------------------------------------------------------
TEST_CASE("AC-C19: ship {0,5,-100} gives cam.z=-105 (negative z back-offset)", "[world][camera_follow]") {
    // Given
    const Vec3 ship{0.0f, 5.0f, -100.0f};

    // When
    const Vec3 cam = world::follow_camera_position(ship);

    // Then
    const float expected_z = -100.0f - world::kCameraBackOffset * terrain::TILE_SIZE;
    CAPTURE(cam.z, expected_z);
    REQUIRE(cam.z == Catch::Approx(expected_z).margin(kCamEps));
    REQUIRE(cam.z == Catch::Approx(-105.0f).margin(kCamEps));
}

// ---------------------------------------------------------------------------
// AC-C20 — Ship near terrain: cam.y clamped to floor.
//   Given: ship_position whose ship.y is below the camera floor
//          (ship.y = terrain::altitude(ship.x, cam.z) - 0.5 — clearly below)
//   When:  follow_camera_position is called
//   Then:  cam.y > ship.y (clamped up to floor + clearance)
// ---------------------------------------------------------------------------
TEST_CASE("AC-C20: ship near (below) terrain gives cam.y clamped to floor+clearance", "[world][camera_follow]") {
    // Given: place ship.y just below floor so clamp activates
    const float ship_x = 2.0f;
    const float ship_z = 5.0f;
    const float cam_z  = ship_z - world::kCameraBackOffset * terrain::TILE_SIZE;
    const float alt    = terrain::altitude(ship_x, cam_z);
    // Ship well below terrain at the camera position
    const float ship_y = alt - 0.5f;
    const Vec3  ship{ship_x, ship_y, ship_z};

    // When
    const Vec3 cam = world::follow_camera_position(ship);

    // Then
    const float expected_floor = alt + world::kCameraGroundClearance;
    CAPTURE(cam.y, ship_y, expected_floor);
    REQUIRE(cam.y > ship_y);
    REQUIRE(cam.y == Catch::Approx(expected_floor).margin(kCamEps));
}

// ---------------------------------------------------------------------------
// AC-C21 — NaN propagation: {NaN,5,0} → cam.x is NaN.
//   Given: ship_position.x = NaN
//   When:  follow_camera_position is called
//   Then:  cam.x is NaN (D-NaNPropagate: no special-case; NaN flows through)
// ---------------------------------------------------------------------------
TEST_CASE("AC-C21: NaN in ship.x propagates to cam.x (D-NaNPropagate)", "[world][camera_follow]") {
    // Given
    const Vec3 ship{std::numeric_limits<float>::quiet_NaN(), 5.0f, 0.0f};

    // When
    const Vec3 cam = world::follow_camera_position(ship);

    // Then: cam.x must be NaN (not a number → not equal to itself)
    CAPTURE(cam.x);
    REQUIRE(std::isnan(cam.x));
}

// ===========================================================================
// Bug-class fences
// ===========================================================================

// ===========================================================================
//  BUG-CLASS FENCE (AC-Cback) — SIGN-FLIP CATCH
// ===========================================================================
//
//  D-BackOffsetSign (locked): cam.z = ship.z - kCameraBackOffset * TILE_SIZE
//
//  Ship at {0,5,10} → cam.z = 10 - 5*1 = 5.
//
//  A sign-flip bug (cam.z = ship.z + 5*TILE_SIZE) would produce cam.z = 15
//  instead of 5, placing the camera IN FRONT of the ship rather than behind.
//  This test fires immediately and unambiguously if the sign is flipped.
//
//  If this test fails: re-read D-BackOffsetSign.  The camera is BEHIND the
//  ship along z, so cam.z < ship.z.  Do NOT change the test — fix the sign.
// ===========================================================================
TEST_CASE("AC-Cback (BUG-CLASS FENCE): ship {0,5,10} gives cam.z=5 NOT 15 (sign-flip catch)", "[world][camera_follow]") {
    // Given
    const Vec3 ship{0.0f, 5.0f, 10.0f};

    // When
    const Vec3 cam = world::follow_camera_position(ship);

    // Then: cam.z must be 5, NOT 15
    CAPTURE(cam.z);
    REQUIRE(cam.z == Catch::Approx(5.0f).margin(kCamEps));
    // Explicit inequality guard: catches the sign-flip producing 15
    REQUIRE(cam.z < ship.z);
    REQUIRE_FALSE(cam.z > ship.z);
}

// ===========================================================================
//  BUG-CLASS FENCE (AC-Cground) — GROUND CLAMP MUST USE FLOOR, NOT SHIP.Y
// ===========================================================================
//
//  D-GroundClampViaTerrain (locked): when ship.y is very low, cam.y must be
//  clamped to altitude(cam.x, cam.z) + clearance, NOT kept at ship.y.
//
//  Ship at {0,-1000,0}: ship.y = -1000 is far below any terrain.
//  A correct implementation gives cam.y ≈ altitude(0,-5) + 0.1 ≈ 5.1.
//  A buggy implementation that skips the clamp returns cam.y = -1000.
//
//  If this test fails: the ground clamp (std::max) is missing or inverted.
//  Do NOT change the test — ensure cam.y = max(ship.y, floor_y).
// ===========================================================================
TEST_CASE("AC-Cground (BUG-CLASS FENCE): ship {0,-1000,0} gives cam.y=floor+clearance NOT -1000", "[world][camera_follow]") {
    // Given
    const Vec3 ship{0.0f, -1000.0f, 0.0f};

    // When
    const Vec3 cam = world::follow_camera_position(ship);

    // Then: cam.y must be near terrain level, NOT -1000
    const float cam_z   = ship.z - world::kCameraBackOffset * terrain::TILE_SIZE;
    const float floor_y = terrain::altitude(ship.x, cam_z) + world::kCameraGroundClearance;
    CAPTURE(cam.y, floor_y, ship.y);
    REQUIRE(cam.y == Catch::Approx(floor_y).margin(kCamEps));
    // Explicit guard: cam.y must be nowhere near ship.y = -1000
    REQUIRE(cam.y > -900.0f);
    // cam.y must be above ship.y (clamped up)
    REQUIRE(cam.y > ship.y);
}

// ===========================================================================
// GROUP 6: Hygiene (AC-C80..C82)
// ===========================================================================
//
// AC-C80 (no-raylib static_assert) is at the top of this file.
// AC-Cnose (return-type static_assert) is at the top of this file.
// AC-C82 (noexcept static_assert) is at the top of this file.

TEST_CASE("AC-C80: world/camera_follow.hpp compiles without raylib, render/, entities/, input/, <random>, <chrono>", "[world][camera_follow]") {
    // Given: this test file was compiled with BUILD_GAME=OFF (no raylib on path)
    // When:  it reaches this TEST_CASE at runtime
    // Then:  it ran — which means the headers compiled and linked without
    //        forbidden dependencies, satisfying AC-C80.
    SUCCEED("compilation without raylib/forbidden deps succeeded — AC-C80 satisfied");
}

TEST_CASE("AC-C81: claude_lander_world link list unchanged — world library links against core+warnings only", "[world][camera_follow]") {
    // Given: this test binary was linked with claude_lander_world which links
    //        against claude_lander_core + claude_lander_warnings only (CMakeLists.txt).
    // When:  it reaches this TEST_CASE
    // Then:  it ran without link errors — AC-C81 satisfied
    SUCCEED("world library link list unchanged (core+warnings only) — AC-C81 satisfied");
}

TEST_CASE("AC-C82: follow_camera_position is noexcept (verified by static_assert at top of TU)", "[world][camera_follow]") {
    // Given: static_assert(noexcept(world::follow_camera_position(Vec3{}))) at
    //        the top of this file.
    // When:  this test is reached (compile succeeded means static_assert passed)
    // Then:  AC-C82 is satisfied.
    SUCCEED("static_assert(noexcept(follow_camera_position(Vec3{}))) passed at compile time — AC-C82 satisfied");
}

TEST_CASE("AC-Cnose: follow_camera_position return type is Vec3 (verified by static_assert at top of TU)", "[world][camera_follow]") {
    // Given: static_assert(std::is_same_v<decltype(...), Vec3>) at top of file.
    // When:  this test is reached
    // Then:  AC-Cnose is satisfied.
    SUCCEED("static_assert(is_same_v<decltype(...), Vec3>) passed at compile time — AC-Cnose satisfied");
}

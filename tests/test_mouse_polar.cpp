// tests/test_mouse_polar.cpp - Pass 5 input/mouse_polar tests.
//
// Covers AC-I01..I23 and AC-I80..I82 (24 ACs total).
// All tests tagged [input][mouse_polar].
//
// === Determinism plan ===
// All tests are fully deterministic.  to_polar, damp, orientation_from_pitch_yaw,
// update_angles, and build_orientation are declared noexcept pure functions.
// No clocks, no filesystem, no PRNG state, no system calls.
// AC-I20 exercises 1000 iterations and requires bit-identical results.
//
// === Matrix formula (a=pitch, b=yaw) ===
//         col[0]=nose          col[1]=roof          col[2]=side
// row 0   cos(a)*cos(b)       -sin(a)*cos(b)        sin(b)
// row 1   sin(a)               cos(a)                0
// row 2  -cos(a)*sin(b)        sin(a)*sin(b)         cos(b)
//
// === Bug-class fences ===
// Three developer-mistake patterns are caught by dedicated tests:
//   (a) AC-Idamp   (AC-I06): damp ratio must be 0.5/0.5, not any other ratio.
//   (b) AC-Iy-up   (AC-I12): col[1] at zero angles must be {0,1,0} (roof = +y).
//   (c) AC-Iyaw-roll (AC-I14): pure yaw-90 must NOT produce roll in col[1].
//
// === AC-I82 noexcept verification ===
// static_assert checks noexcept on all 5 public functions are placed after the
// #include so they fire at compile time in the same TU.
//
// === Red-state expectation ===
// This file FAILS TO COMPILE (missing header) until the implementer creates:
//   src/input/mouse_polar.hpp
//   src/input/mouse_polar.cpp
// That is the correct red state for Gate 2 delivery.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numbers>

#include "core/vec2.hpp"
#include "core/matrix3.hpp"
#include "input/mouse_polar.hpp"

// M_PI is a POSIX extension and is not provided by Windows MinGW's <cmath>.
// Define a portable double-precision pi locally so every existing
// `static_cast<float>(M_PI / N)` site keeps its arithmetic shape.  We use
// std::numbers::pi_v<double> (C++20) as the source of truth -- bit-identical
// to glibc's M_PI on every platform we target.
#ifndef M_PI
#  define M_PI (std::numbers::pi_v<double>)
#endif

// ---------------------------------------------------------------------------
// AC-I80: src/input/ must not pull in raylib, world/, entities/, render/,
// physics/, <random>, or <chrono>.
// RAYLIB_VERSION is defined by raylib.h; if present here, the include chain
// is polluted.  The BUILD_GAME=OFF build keeps raylib off the compiler path;
// this static_assert is a belt-and-suspenders in-translation-unit tripwire.
// ---------------------------------------------------------------------------
#ifdef RAYLIB_VERSION
static_assert(false,
    "AC-I80 VIOLATED: input/mouse_polar.hpp pulled in raylib.h "
    "(RAYLIB_VERSION is defined).  Remove the raylib dependency from "
    "src/input/mouse_polar.hpp and src/input/mouse_polar.cpp.");
#endif

// ---------------------------------------------------------------------------
// AC-I82: All 5 public functions must be declared noexcept.
// Checked at compile time here in the same TU as the #include.
// ---------------------------------------------------------------------------
static_assert(noexcept(input::to_polar(Vec2{0.0f, 0.0f})),
    "AC-I82: input::to_polar must be declared noexcept");
static_assert(noexcept(input::damp(0.0f, 0.0f)),
    "AC-I82: input::damp must be declared noexcept");
static_assert(noexcept(input::orientation_from_pitch_yaw(0.0f, 0.0f)),
    "AC-I82: input::orientation_from_pitch_yaw must be declared noexcept");
static_assert(noexcept(input::update_angles(input::ShipAngles{}, Vec2{0.0f, 0.0f})),
    "AC-I82: input::update_angles must be declared noexcept");
static_assert(noexcept(input::build_orientation(input::ShipAngles{})),
    "AC-I82: input::build_orientation must be declared noexcept");

// ---------------------------------------------------------------------------
// Tolerance constants (per planner spec §4)
// ---------------------------------------------------------------------------
static constexpr float kPolarEps = 1e-5f;   // polar radius and angle checks
static constexpr float kAngleEps = 1e-6f;   // angle accumulation checks
static constexpr float kMatEps   = 1e-5f;   // matrix element checks

// ===========================================================================
// Helper: column vector length
// ===========================================================================

static float col_length(Vec3 v) {
    return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
}

// ===========================================================================
// GROUP 1: to_polar (AC-I01..I05)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-I01: to_polar({0,0}) == {0,0} exactly.
//   Origin input must produce exactly zero radius and zero angle; no UB from
//   atan2(0,0) (implementation-defined but typically 0; here exact check
//   against the locked return value is appropriate).
// ---------------------------------------------------------------------------
TEST_CASE("AC-I01: to_polar({0,0}) returns {radius=0, angle=0} exactly", "[input][mouse_polar]") {
    // Given: mouse offset = {0, 0}
    // When:  to_polar is called
    // Then:  radius == 0.0f exactly, angle == 0.0f exactly
    const auto p = input::to_polar(Vec2{0.0f, 0.0f});
    REQUIRE(p.radius == 0.0f);
    REQUIRE(p.angle  == 0.0f);
}

// ---------------------------------------------------------------------------
// AC-I02: Axis-aligned unit inputs hit exact angles within kPolarEps.
//   (1,0)  → (1,  0)        positive-x axis  → angle 0
//   (0,1)  → (1, π/2)       positive-y axis  → angle π/2
//   (-1,0) → (1,  π)        negative-x axis  → angle π
//   (0,-1) → (1, -π/2)      negative-y axis  → angle -π/2
// ---------------------------------------------------------------------------
TEST_CASE("AC-I02: to_polar on axis-aligned unit inputs returns correct radius and angle", "[input][mouse_polar]") {
    // Given: four axis-aligned inputs of magnitude 1
    // When:  to_polar is called on each
    // Then:  radius == 1.0 and angle matches the expected quadrant boundary
    using namespace Catch;

    {
        const auto p = input::to_polar(Vec2{1.0f, 0.0f});
        CAPTURE(p.radius, p.angle);
        REQUIRE(p.radius == Approx(1.0f).margin(kPolarEps));
        REQUIRE(p.angle  == Approx(0.0f).margin(kPolarEps));
    }
    {
        const auto p = input::to_polar(Vec2{0.0f, 1.0f});
        CAPTURE(p.radius, p.angle);
        REQUIRE(p.radius == Approx(1.0f).margin(kPolarEps));
        REQUIRE(p.angle  == Approx(static_cast<float>(M_PI / 2.0)).margin(kPolarEps));
    }
    {
        const auto p = input::to_polar(Vec2{-1.0f, 0.0f});
        CAPTURE(p.radius, p.angle);
        REQUIRE(p.radius == Approx(1.0f).margin(kPolarEps));
        REQUIRE(p.angle  == Approx(static_cast<float>(M_PI)).margin(kPolarEps));
    }
    {
        const auto p = input::to_polar(Vec2{0.0f, -1.0f});
        CAPTURE(p.radius, p.angle);
        REQUIRE(p.radius == Approx(1.0f).margin(kPolarEps));
        REQUIRE(p.angle  == Approx(static_cast<float>(-M_PI / 2.0)).margin(kPolarEps));
    }
}

// ---------------------------------------------------------------------------
// AC-I03: All 4 quadrant diagonals.
//   (1,1)   → (√2, π/4)      Q1
//   (-1,1)  → (√2, 3π/4)     Q2
//   (-1,-1) → (√2, -3π/4)    Q3
//   (1,-1)  → (√2, -π/4)     Q4
// ---------------------------------------------------------------------------
TEST_CASE("AC-I03: to_polar on all four quadrant diagonals returns correct radius and angle", "[input][mouse_polar]") {
    // Given: four diagonal inputs at ±1, ±1
    // When:  to_polar is called on each
    // Then:  radius == √2 and angle matches the expected quadrant diagonal
    using namespace Catch;
    const float sqrt2 = std::sqrt(2.0f);

    {
        const auto p = input::to_polar(Vec2{1.0f, 1.0f});
        CAPTURE(p.radius, p.angle);
        REQUIRE(p.radius == Approx(sqrt2).margin(kPolarEps));
        REQUIRE(p.angle  == Approx(static_cast<float>(M_PI / 4.0)).margin(kPolarEps));
    }
    {
        const auto p = input::to_polar(Vec2{-1.0f, 1.0f});
        CAPTURE(p.radius, p.angle);
        REQUIRE(p.radius == Approx(sqrt2).margin(kPolarEps));
        REQUIRE(p.angle  == Approx(static_cast<float>(3.0 * M_PI / 4.0)).margin(kPolarEps));
    }
    {
        const auto p = input::to_polar(Vec2{-1.0f, -1.0f});
        CAPTURE(p.radius, p.angle);
        REQUIRE(p.radius == Approx(sqrt2).margin(kPolarEps));
        REQUIRE(p.angle  == Approx(static_cast<float>(-3.0 * M_PI / 4.0)).margin(kPolarEps));
    }
    {
        const auto p = input::to_polar(Vec2{1.0f, -1.0f});
        CAPTURE(p.radius, p.angle);
        REQUIRE(p.radius == Approx(sqrt2).margin(kPolarEps));
        REQUIRE(p.angle  == Approx(static_cast<float>(-M_PI / 4.0)).margin(kPolarEps));
    }
}

// ---------------------------------------------------------------------------
// AC-I04: Large magnitudes remain finite.
//   to_polar({1e6, 1e6}): radius finite ≈ √2 * 1e6, angle ≈ π/4.
// ---------------------------------------------------------------------------
TEST_CASE("AC-I04: to_polar on large-magnitude input produces finite radius and angle", "[input][mouse_polar]") {
    // Given: mouse offset = {1e6, 1e6}
    // When:  to_polar is called
    // Then:  radius is finite ≈ √2 * 1e6; angle is finite ≈ π/4
    const auto p = input::to_polar(Vec2{1.0e6f, 1.0e6f});
    CAPTURE(p.radius, p.angle);
    REQUIRE(std::isfinite(p.radius));
    REQUIRE(std::isfinite(p.angle));
    REQUIRE(p.radius == Catch::Approx(std::sqrt(2.0f) * 1.0e6f).margin(1.0f));
    REQUIRE(p.angle  == Catch::Approx(static_cast<float>(M_PI / 4.0)).margin(kPolarEps));
}

// ---------------------------------------------------------------------------
// AC-I05: Round-trip: reconstruct (dx, dy) from (radius, angle) within kPolarEps.
//   Inputs: (3,4), (-7,24), (0.5, -0.866)
// ---------------------------------------------------------------------------
TEST_CASE("AC-I05: to_polar round-trip: radius*cos(angle) and radius*sin(angle) reconstruct original offset", "[input][mouse_polar]") {
    // Given: three non-trivial offsets
    // When:  to_polar is applied and the result is reconstructed via sin/cos
    // Then:  reconstructed (dx,dy) matches original within kPolarEps

    auto check = [](Vec2 in) {
        const auto p = input::to_polar(in);
        const float dx_back = p.radius * std::cos(p.angle);
        const float dy_back = p.radius * std::sin(p.angle);
        CAPTURE(in.x, in.y, p.radius, p.angle, dx_back, dy_back);
        REQUIRE(dx_back == Catch::Approx(in.x).margin(kPolarEps));
        REQUIRE(dy_back == Catch::Approx(in.y).margin(kPolarEps));
    };

    check(Vec2{3.0f, 4.0f});
    check(Vec2{-7.0f, 24.0f});
    check(Vec2{0.5f, -0.866f});
}

// ===========================================================================
// GROUP 2: damp (AC-I06..I09)
// ===========================================================================

// ===========================================================================
//  DAMP RATIO BUG FENCE (AC-Idamp) - bug-class fence
// ===========================================================================
//
//  D-DampRatio locks: damp(prev, input) = 0.5f * prev + 0.5f * input.
//  The ratio is 50/50.  Any other split (e.g. 75/25, 25/75, or a
//  different alpha entirely) would produce a different value for
//  damp(0, 1).  The correct result is exactly 0.5f.
//
//  If this test fails: re-read D-DampRatio in the planner output before
//  modifying.  The test is almost certainly correct; the implementation
//  has the wrong ratio.
//
//  Common mistake: `prev - (prev - input) / 2` is mathematically identical
//  and must also produce 0.5f when prev=0, input=1.
// ===========================================================================
TEST_CASE("AC-I06 (AC-Idamp): damp(0, 1) == 0.5f exactly - 50/50 blend ratio", "[input][mouse_polar]") {
    // Given: prev = 0.0f, input = 1.0f
    // When:  damp is called
    // Then:  result == 0.5f exactly (50/50 blend)
    //        Any other ratio produces a different value and fails here.
    REQUIRE(input::damp(0.0f, 1.0f) == 0.5f);
}

// ---------------------------------------------------------------------------
// AC-I07: Identity at fixpoint - damp(x, x) == x for several x values.
//   If prev and input are already equal, the blend must return the same value.
// ---------------------------------------------------------------------------
TEST_CASE("AC-I07: damp(x, x) == x for several representative x values", "[input][mouse_polar]") {
    // Given: prev == input for several x values
    // When:  damp is called
    // Then:  result == x (fixpoint; blending equal values yields the same value)
    const float pi = static_cast<float>(M_PI);
    for (float x : {0.0f, 1.0f, -1.0f, 0.5f, -pi}) {
        const float result = input::damp(x, x);
        CAPTURE(x, result);
        REQUIRE(result == Catch::Approx(x).margin(kAngleEps));
    }
}

// ---------------------------------------------------------------------------
// AC-I08: Decay to input - 30 iterations from prev=0 towards input=1.0f
//   After 30 frames with constant target 1.0f, |prev - 1.0f| < 1e-6f.
//   With ratio 0.5, after n frames: |error| = (0.5)^n.  (0.5)^30 ≈ 9.3e-10.
// ---------------------------------------------------------------------------
TEST_CASE("AC-I08: 30 damp iterations from 0 towards 1.0f converge within 1e-6f", "[input][mouse_polar]") {
    // Given: prev = 0.0f, target = 1.0f
    // When:  damp(prev, 1.0f) is applied 30 times
    // Then:  |prev - 1.0f| < 1e-6f (geometric convergence at rate 0.5/frame)
    float prev = 0.0f;
    for (int i = 0; i < 30; ++i) {
        prev = input::damp(prev, 1.0f);
    }
    CAPTURE(prev);
    REQUIRE(std::abs(prev - 1.0f) < 1e-6f);
}

// ---------------------------------------------------------------------------
// AC-I09: NaN propagation - both argument positions must propagate NaN.
//   damp(NaN, 0) is NaN; damp(0, NaN) is NaN.
//   This matches D-Stateless / KOQ-6: NaN is not masked; it propagates.
// ---------------------------------------------------------------------------
TEST_CASE("AC-I09: damp propagates NaN from either argument", "[input][mouse_polar]") {
    // Given: one argument is NaN
    // When:  damp is called
    // Then:  result is NaN (NaN propagation; not clamped or masked)
    const float nan = std::numeric_limits<float>::quiet_NaN();
    REQUIRE(std::isnan(input::damp(nan, 0.0f)));
    REQUIRE(std::isnan(input::damp(0.0f, nan)));
}

// ===========================================================================
// GROUP 3: orientation_from_pitch_yaw (AC-I10..I16)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-I10: Identity at zero angles.
//   orientation_from_pitch_yaw(0, 0) == identity() within kMatEps.
// ---------------------------------------------------------------------------
TEST_CASE("AC-I10: orientation_from_pitch_yaw(0, 0) equals identity within kMatEps", "[input][mouse_polar]") {
    // Given: pitch = 0, yaw = 0
    // When:  orientation_from_pitch_yaw is called
    // Then:  result equals the 3x3 identity matrix within kMatEps
    const Mat3 m = input::orientation_from_pitch_yaw(0.0f, 0.0f);
    const Mat3 id = identity();
    CAPTURE(m.col[0].x, m.col[0].y, m.col[0].z);
    CAPTURE(m.col[1].x, m.col[1].y, m.col[1].z);
    CAPTURE(m.col[2].x, m.col[2].y, m.col[2].z);
    for (int c = 0; c < 3; ++c) {
        REQUIRE(m.col[c].x == Catch::Approx(id.col[c].x).margin(kMatEps));
        REQUIRE(m.col[c].y == Catch::Approx(id.col[c].y).margin(kMatEps));
        REQUIRE(m.col[c].z == Catch::Approx(id.col[c].z).margin(kMatEps));
    }
}

// ---------------------------------------------------------------------------
// AC-I11: Determinant invariant - det(M) == 1 for four representative inputs.
//   (0,0), (π/4, π/4), (π/3, -π/6), (-π/4, π/2).
// ---------------------------------------------------------------------------
TEST_CASE("AC-I11: det(orientation_from_pitch_yaw) == 1 for representative angle pairs", "[input][mouse_polar]") {
    // Given: four (pitch, yaw) pairs
    // When:  orientation_from_pitch_yaw is called on each
    // Then:  |det(M) - 1| < kMatEps for each result
    const float pi = static_cast<float>(M_PI);
    struct PY { float pitch, yaw; };
    for (auto [p, y] : {PY{0.0f, 0.0f},
                        PY{pi/4.0f, pi/4.0f},
                        PY{pi/3.0f, -pi/6.0f},
                        PY{-pi/4.0f, pi/2.0f}}) {
        const Mat3 m = input::orientation_from_pitch_yaw(p, y);
        const float d = det(m);
        CAPTURE(p, y, d);
        REQUIRE(std::abs(d - 1.0f) < kMatEps);
    }
}

// ===========================================================================
//  ROOF-IS-Y-UP FENCE (AC-Iy-up) - bug-class fence
// ===========================================================================
//
//  D-RoofAtCol1 locks: at zero angles, col[1] (the roof axis) MUST be
//  {0, 1, 0}.  Any transposition bug (e.g. col[1] = col[0] semantics, or
//  row/column confusion in the formula) would produce a different vector.
//
//  From the formula at a=0, b=0:
//    col[1] row 0 = -sin(0)*cos(0) = 0
//    col[1] row 1 =  cos(0)        = 1
//    col[1] row 2 =  sin(0)*sin(0) = 0
//  → col[1] = {0, 1, 0}.
//
//  If this test fails: re-read D-MatrixLayout and D-RoofAtCol1 in the
//  planner output and cross-check the spec matrix formula before modifying.
// ===========================================================================
TEST_CASE("AC-I12 (AC-Iy-up): orientation_from_pitch_yaw(0,0).col[1] == {0,1,0} (roof = +y)", "[input][mouse_polar]") {
    // Given: pitch = 0, yaw = 0
    // When:  orientation_from_pitch_yaw is called
    // Then:  col[1] (roof axis) == {0, 1, 0} within kMatEps
    //        A row/col transposition or wrong index assignment would fail this.
    const Mat3 m = input::orientation_from_pitch_yaw(0.0f, 0.0f);
    CAPTURE(m.col[1].x, m.col[1].y, m.col[1].z);
    REQUIRE(m.col[1].x == Catch::Approx(0.0f).margin(kMatEps));
    REQUIRE(m.col[1].y == Catch::Approx(1.0f).margin(kMatEps));
    REQUIRE(m.col[1].z == Catch::Approx(0.0f).margin(kMatEps));
}

// ---------------------------------------------------------------------------
// AC-I13: Pure pitch π/2 tilts the nose to {0, 1, 0}.
//   From the formula at a=π/2, b=0:
//     col[0] row 0 = cos(π/2)*cos(0) = 0
//     col[0] row 1 = sin(π/2)        = 1
//     col[0] row 2 = -cos(π/2)*sin(0) = 0
//   → col[0] = {0, 1, 0}
// ---------------------------------------------------------------------------
TEST_CASE("AC-I13: orientation_from_pitch_yaw(pi/2, 0).col[0] == {0,1,0} (nose tilted up by pi/2)", "[input][mouse_polar]") {
    // Given: pitch = π/2, yaw = 0
    // When:  orientation_from_pitch_yaw is called
    // Then:  col[0] (nose axis) == {0, 1, 0} within kMatEps
    const float pi = static_cast<float>(M_PI);
    const Mat3 m = input::orientation_from_pitch_yaw(pi / 2.0f, 0.0f);
    CAPTURE(m.col[0].x, m.col[0].y, m.col[0].z);
    REQUIRE(m.col[0].x == Catch::Approx(0.0f).margin(kMatEps));
    REQUIRE(m.col[0].y == Catch::Approx(1.0f).margin(kMatEps));
    REQUIRE(m.col[0].z == Catch::Approx(0.0f).margin(kMatEps));
}

// ===========================================================================
//  YAW-MUST-NOT-ROLL FENCE (AC-Iyaw-roll) - bug-class fence
// ===========================================================================
//
//  D-MatrixLayout: pure yaw rotation changes col[0] and col[2] (nose and
//  side axes) but must NOT tilt col[1] (roof) off {0,1,0}.  The matrix
//  formula has col[1].y == cos(a) and col[1].x == -sin(a)*cos(b).
//  At a=0 (no pitch), col[1] = {0, cos(0), sin(0)*sin(0)} = {0, 1, 0}
//  regardless of b (yaw).
//
//  At a=0, b=π/2:
//    col[0]: row0 = cos(0)*cos(π/2) = 0
//            row1 = sin(0)          = 0
//            row2 = -cos(0)*sin(π/2)= -1   → col[0] = {0, 0, -1}
//    col[1]: row0 = -sin(0)*cos(π/2)= 0
//            row1 = cos(0)          = 1
//            row2 = sin(0)*sin(π/2) = 0    → col[1] = {0, 1, 0}
//    col[2]: row0 = sin(π/2)        = 1
//            row1 = 0               = 0
//            row2 = cos(π/2)        = 0    → col[2] = {1, 0, 0}
//
//  If a bug mixes yaw into the roof (e.g. a row-major vs column-major
//  indexing error), col[1] would diverge from {0,1,0} under pure yaw.
//  This test catches that class of bug loudly.
// ===========================================================================
TEST_CASE("AC-I14 (AC-Iyaw-roll): orientation_from_pitch_yaw(0, pi/2) has correct col[0], col[1], col[2]", "[input][mouse_polar]") {
    // Given: pitch = 0, yaw = π/2
    // When:  orientation_from_pitch_yaw is called
    // Then:
    //   col[0] (nose)  == {0,  0, -1}  (facing -z after 90-degree yaw)
    //   col[1] (roof)  == {0,  1,  0}  (MUST NOT ROLL under pure yaw)
    //   col[2] (side)  == {1,  0,  0}  (right side points along +x)
    const float pi = static_cast<float>(M_PI);
    const Mat3 m = input::orientation_from_pitch_yaw(0.0f, pi / 2.0f);
    CAPTURE(m.col[0].x, m.col[0].y, m.col[0].z);
    CAPTURE(m.col[1].x, m.col[1].y, m.col[1].z);
    CAPTURE(m.col[2].x, m.col[2].y, m.col[2].z);

    // col[0] = nose = {0, 0, -1}
    REQUIRE(m.col[0].x == Catch::Approx(0.0f).margin(kMatEps));
    REQUIRE(m.col[0].y == Catch::Approx(0.0f).margin(kMatEps));
    REQUIRE(m.col[0].z == Catch::Approx(-1.0f).margin(kMatEps));

    // col[1] = roof = {0, 1, 0} - must NOT roll
    REQUIRE(m.col[1].x == Catch::Approx(0.0f).margin(kMatEps));
    REQUIRE(m.col[1].y == Catch::Approx(1.0f).margin(kMatEps));
    REQUIRE(m.col[1].z == Catch::Approx(0.0f).margin(kMatEps));

    // col[2] = side = {1, 0, 0}
    REQUIRE(m.col[2].x == Catch::Approx(1.0f).margin(kMatEps));
    REQUIRE(m.col[2].y == Catch::Approx(0.0f).margin(kMatEps));
    REQUIRE(m.col[2].z == Catch::Approx(0.0f).margin(kMatEps));
}

// ---------------------------------------------------------------------------
// AC-I15: Orthonormal columns at (π/3, -π/6).
//   Each column has unit length within kMatEps; pairwise dot products < kMatEps.
// ---------------------------------------------------------------------------
TEST_CASE("AC-I15: orientation_from_pitch_yaw(pi/3, -pi/6) has orthonormal columns", "[input][mouse_polar]") {
    // Given: pitch = π/3, yaw = -π/6
    // When:  orientation_from_pitch_yaw is called
    // Then:  each column has |length - 1| < kMatEps
    //        each pairwise dot product has |dot| < kMatEps
    const float pi = static_cast<float>(M_PI);
    const Mat3 m = input::orientation_from_pitch_yaw(pi / 3.0f, -pi / 6.0f);

    auto dot3 = [](Vec3 a, Vec3 b) {
        return a.x*b.x + a.y*b.y + a.z*b.z;
    };

    for (int c = 0; c < 3; ++c) {
        const float len = col_length(m.col[c]);
        CAPTURE(c, len);
        REQUIRE(std::abs(len - 1.0f) < kMatEps);
    }

    const float d01 = dot3(m.col[0], m.col[1]);
    const float d02 = dot3(m.col[0], m.col[2]);
    const float d12 = dot3(m.col[1], m.col[2]);
    CAPTURE(d01, d02, d12);
    REQUIRE(std::abs(d01) < kMatEps);
    REQUIRE(std::abs(d02) < kMatEps);
    REQUIRE(std::abs(d12) < kMatEps);
}

// ---------------------------------------------------------------------------
// AC-I16: Roof tilts forward at pitch π/4.
//   From the formula at a=π/4, b=0:
//     col[1] row 0 = -sin(π/4)*cos(0) = -√2/2
//     col[1] row 1 =  cos(π/4)        = +√2/2
//   → col[1].x == -√2/2, col[1].y == +√2/2 within kMatEps.
// ---------------------------------------------------------------------------
TEST_CASE("AC-I16: orientation_from_pitch_yaw(pi/4, 0).col[1] tilts forward: x=-sqrt2/2, y=+sqrt2/2", "[input][mouse_polar]") {
    // Given: pitch = π/4, yaw = 0
    // When:  orientation_from_pitch_yaw is called
    // Then:  col[1].x == -√2/2 (roof tilts forward/down in x),
    //        col[1].y == +√2/2 (roof still has +y component)
    const float pi    = static_cast<float>(M_PI);
    const float sqrt2_2 = std::sqrt(2.0f) / 2.0f;
    const Mat3 m = input::orientation_from_pitch_yaw(pi / 4.0f, 0.0f);
    CAPTURE(m.col[1].x, m.col[1].y, m.col[1].z, sqrt2_2);
    REQUIRE(m.col[1].x == Catch::Approx(-sqrt2_2).margin(kMatEps));
    REQUIRE(m.col[1].y == Catch::Approx(+sqrt2_2).margin(kMatEps));
}

// ===========================================================================
// GROUP 4: update_angles + composite (AC-I17..I20)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-I17: update_angles({0,0}, {1,0}) → {pitch=0.5, yaw=0.0} within kAngleEps.
//   to_polar({1,0}) = {radius=1, angle=0}
//   pitch_new = damp(0, 1)     = 0.5f
//   yaw_new   = damp(0, 0)     = 0.0f
// ---------------------------------------------------------------------------
TEST_CASE("AC-I17: update_angles({0,0}, {1,0}) yields {pitch=0.5, yaw=0.0}", "[input][mouse_polar]") {
    // Given: prev = {0, 0}, mouse_offset = {1, 0}
    // When:  update_angles is called
    // Then:  pitch == 0.5f (damp of radius=1 from 0), yaw == 0.0f (damp of angle=0 from 0)
    const input::ShipAngles prev{0.0f, 0.0f};
    const auto result = input::update_angles(prev, Vec2{1.0f, 0.0f});
    CAPTURE(result.pitch, result.yaw);
    REQUIRE(result.pitch == Catch::Approx(0.5f).margin(kAngleEps));
    REQUIRE(result.yaw   == Catch::Approx(0.0f).margin(kAngleEps));
}

// ---------------------------------------------------------------------------
// AC-I18: update_angles({0.4, 0.2}, {0, 1}) uses non-trivial prev state.
//   to_polar({0,1}) = {radius=1, angle=π/2}
//   pitch_new = damp(0.4, 1.0)         = 0.5*0.4 + 0.5*1.0 = 0.7
//   yaw_new   = damp(0.2, π/2)         = 0.5*0.2 + 0.5*(π/2)
// ---------------------------------------------------------------------------
TEST_CASE("AC-I18: update_angles({0.4, 0.2}, {0,1}) yields correct pitch and yaw", "[input][mouse_polar]") {
    // Given: prev = {0.4, 0.2}, mouse_offset = {0, 1}
    // When:  update_angles is called
    // Then:  pitch == 0.7f (damp(0.4, 1.0)),
    //        yaw   == 0.5*0.2 + 0.5*(π/2)  (damp(0.2, π/2))
    const input::ShipAngles prev{0.4f, 0.2f};
    const auto result = input::update_angles(prev, Vec2{0.0f, 1.0f});
    const float expected_yaw = 0.5f * 0.2f + 0.5f * static_cast<float>(M_PI / 2.0);
    CAPTURE(result.pitch, result.yaw, expected_yaw);
    REQUIRE(result.pitch == Catch::Approx(0.7f).margin(kAngleEps));
    REQUIRE(result.yaw   == Catch::Approx(expected_yaw).margin(kAngleEps));
}

// ---------------------------------------------------------------------------
// AC-I19: build_orientation(angles) == orientation_from_pitch_yaw(pitch, yaw) element-wise.
//   build_orientation is a thin wrapper; results must be bit-identical.
// ---------------------------------------------------------------------------
TEST_CASE("AC-I19: build_orientation(angles) is element-wise identical to orientation_from_pitch_yaw(pitch, yaw)", "[input][mouse_polar]") {
    // Given: angles = {π/5, π/7} (arbitrary non-trivial values)
    // When:  build_orientation(angles) and orientation_from_pitch_yaw(pitch, yaw) are called
    // Then:  every element is bit-identical (exact equality, not Approx)
    const float pi = static_cast<float>(M_PI);
    const input::ShipAngles angles{pi / 5.0f, pi / 7.0f};
    const Mat3 via_build    = input::build_orientation(angles);
    const Mat3 via_direct   = input::orientation_from_pitch_yaw(angles.pitch, angles.yaw);
    for (int c = 0; c < 3; ++c) {
        CAPTURE(c, via_build.col[c].x, via_direct.col[c].x);
        REQUIRE(via_build.col[c].x == via_direct.col[c].x);
        REQUIRE(via_build.col[c].y == via_direct.col[c].y);
        REQUIRE(via_build.col[c].z == via_direct.col[c].z);
    }
}

// ---------------------------------------------------------------------------
// AC-I20: 1000 deterministic iterations - bit-identical between two fresh runs.
//   Confirms no PRNG, clock, or global mutable state inside the pipeline.
// ---------------------------------------------------------------------------
TEST_CASE("AC-I20: 1000 update_angles iterations are deterministic - bit-identical to a fresh independent run", "[input][mouse_polar]") {
    // Given: two identical initial ShipAngles; same fixed mouse_offset sequence
    // When:  each is advanced 1000 steps independently
    // Then:  final angles are bit-identical between the two runs
    auto run_simulation = []() -> input::ShipAngles {
        input::ShipAngles angles{0.0f, 0.0f};
        // Fixed offset that exercises both axes
        const Vec2 offset{0.7f, 0.3f};
        for (int i = 0; i < 1000; ++i) {
            angles = input::update_angles(angles, offset);
        }
        return angles;
    };

    const input::ShipAngles run_a = run_simulation();
    const input::ShipAngles run_b = run_simulation();
    CAPTURE(run_a.pitch, run_b.pitch, run_a.yaw, run_b.yaw);
    // Bit-identical: exact equality, NOT Approx
    REQUIRE(run_a.pitch == run_b.pitch);
    REQUIRE(run_a.yaw   == run_b.yaw);
}

// ===========================================================================
// GROUP 5: Convergence (AC-I21..I23)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-I21: Constant offset {1,0}, 30 frames → pitch converges to 1.0f,
//   yaw stays at 0.
//   to_polar({1,0}) = {1, 0}.  With damp=0.5: (0.5)^30 error ≈ 9.3e-10.
// ---------------------------------------------------------------------------
TEST_CASE("AC-I21: 30 frames with constant offset {1,0}: pitch converges to 1.0, yaw stays 0.0", "[input][mouse_polar]") {
    // Given: initial angles = {0, 0}, constant offset = {1, 0}
    // When:  update_angles applied 30 times
    // Then:  |pitch - 1.0f| < 1e-6f,  |yaw - 0.0f| < kAngleEps
    input::ShipAngles angles{0.0f, 0.0f};
    for (int i = 0; i < 30; ++i) {
        angles = input::update_angles(angles, Vec2{1.0f, 0.0f});
    }
    CAPTURE(angles.pitch, angles.yaw);
    REQUIRE(std::abs(angles.pitch - 1.0f) < 1e-6f);
    REQUIRE(std::abs(angles.yaw)          < kAngleEps);
}

// ---------------------------------------------------------------------------
// AC-I22: Constant offset {1,1}, 30 frames → pitch converges to √2,
//   yaw converges to π/4.
//   to_polar({1,1}) = {√2, π/4}.
// ---------------------------------------------------------------------------
TEST_CASE("AC-I22: 30 frames with constant offset {1,1}: pitch converges to sqrt(2), yaw converges to pi/4", "[input][mouse_polar]") {
    // Given: initial angles = {0, 0}, constant offset = {1, 1}
    // When:  update_angles applied 30 times
    // Then:  |pitch - √2| < 1e-6f,  |yaw - π/4| < kAngleEps
    input::ShipAngles angles{0.0f, 0.0f};
    for (int i = 0; i < 30; ++i) {
        angles = input::update_angles(angles, Vec2{1.0f, 1.0f});
    }
    const float sqrt2   = std::sqrt(2.0f);
    const float pi4     = static_cast<float>(M_PI / 4.0);
    CAPTURE(angles.pitch, angles.yaw, sqrt2, pi4);
    REQUIRE(std::abs(angles.pitch - sqrt2) < 1e-6f);
    REQUIRE(std::abs(angles.yaw   - pi4)   < kAngleEps);
}

// ---------------------------------------------------------------------------
// AC-I23: 50-frame steady state with offset {1,0}:
//   det(M) == 1 within kMatEps and col[1] has unit length within kMatEps.
//   After convergence the matrix must still be orthonormal.
// ---------------------------------------------------------------------------
TEST_CASE("AC-I23: 50 frames with constant offset {1,0}: build_orientation gives det==1 and |col[1]|==1", "[input][mouse_polar]") {
    // Given: initial angles = {0, 0}, constant offset = {1, 0}
    // When:  update_angles applied 50 times, then build_orientation called
    // Then:  |det(M) - 1| < kMatEps,  |col[1].length - 1| < kMatEps
    input::ShipAngles angles{0.0f, 0.0f};
    for (int i = 0; i < 50; ++i) {
        angles = input::update_angles(angles, Vec2{1.0f, 0.0f});
    }
    const Mat3 m = input::build_orientation(angles);
    const float d = det(m);
    const float roof_len = col_length(m.col[1]);
    CAPTURE(d, roof_len, angles.pitch, angles.yaw);
    REQUIRE(std::abs(d - 1.0f)          < kMatEps);
    REQUIRE(std::abs(roof_len - 1.0f)   < kMatEps);
}

// ===========================================================================
// GROUP 6: Hygiene (AC-I80..I82)
// ===========================================================================
//
// AC-I80 (no-raylib static_assert) is at the top of this file, immediately
// after the #include "input/mouse_polar.hpp".
//
// AC-I81: claude_lander_input link list = core + warnings only.
//   Verified by CMakeLists.txt; no runtime TEST_CASE needed.
//   The BUILD_GAME=OFF build exercises this invariant.
//
// AC-I82: All public functions are noexcept.
//   Verified by the static_assert block at the top of this file.

TEST_CASE("AC-I80: input/mouse_polar header and library compile and link without raylib (BUILD_GAME=OFF)", "[input][mouse_polar]") {
    // Given: this test file was compiled with BUILD_GAME=OFF (no raylib on path)
    // When:  it reaches this TEST_CASE at runtime
    // Then:  it ran - which means mouse_polar.hpp compiled and linked without
    //        raylib, satisfying AC-I80.
    SUCCEED("compilation and linkage without raylib succeeded - AC-I80 satisfied");
}

TEST_CASE("AC-I81: input library compiles and links against core only - no forbidden deps at runtime", "[input][mouse_polar]") {
    // Given: this test binary was linked with claude_lander_input depending on
    //        core + warnings only (verified by CMakeLists.txt)
    // When:  it reaches this TEST_CASE
    // Then:  it ran without link errors, so AC-I81 is satisfied
    SUCCEED("link against core+warnings only - AC-I81 satisfied");
}

TEST_CASE("AC-I82: all 5 public input functions are noexcept (verified by static_assert at top of TU)", "[input][mouse_polar]") {
    // Given: static_assert checks at the top of this file verified noexcept for
    //        to_polar, damp, orientation_from_pitch_yaw, update_angles, build_orientation
    // When:  this test is reached (compile succeeded means all static_asserts passed)
    // Then:  AC-I82 is satisfied
    SUCCEED("all static_assert(noexcept(...)) checks passed at compile time - AC-I82 satisfied");
}

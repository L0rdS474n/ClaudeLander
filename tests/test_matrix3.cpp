// tests/test_matrix3.cpp - Pass 1 Mat3 math layer tests.
//
// Covers AC-M01 through AC-M10.
// All tests are deterministic: no clocks, no filesystem, no random_device.
// All float comparisons use explicit epsilon (1e-5f) or Catch::Approx.
//
// Column-major convention (bbcelite Lander):
//   col[0] = nose axis, col[1] = roof axis, col[2] = side axis
//   at(m, row, col) → m.col[col].{x,y,z} for row in {0,1,2}

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

#include "core/matrix3.hpp"   // also pulls in vec3.hpp

static constexpr float kEps     = 1e-5f;
static constexpr float kEpsMed  = 1e-5f;   // kept named for readability

// Helper: compare two Mat3 element-wise within eps
static bool mat_approx_equal(const Mat3& a, const Mat3& b, float eps = kEps) {
    for (int c = 0; c < 3; ++c) {
        if (!approx_equal(a.col[c], b.col[c], eps)) return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// AC-M01: identity() returns the identity matrix
// Given: identity matrix constructed via identity()
// When:  element at(m, r, c) for r==c checked
// Then:  diagonal elements == 1.0f, off-diagonal == 0.0f
// ---------------------------------------------------------------------------
TEST_CASE("AC-M01: identity returns the 3x3 identity matrix", "[core][matrix3]") {
    constexpr Mat3 m = identity();
    // Diagonal
    REQUIRE(at(m, 0, 0) == 1.0f);
    REQUIRE(at(m, 1, 1) == 1.0f);
    REQUIRE(at(m, 2, 2) == 1.0f);
    // Off-diagonal
    REQUIRE(at(m, 0, 1) == 0.0f);
    REQUIRE(at(m, 0, 2) == 0.0f);
    REQUIRE(at(m, 1, 0) == 0.0f);
    REQUIRE(at(m, 1, 2) == 0.0f);
    REQUIRE(at(m, 2, 0) == 0.0f);
    REQUIRE(at(m, 2, 1) == 0.0f);
}

// ---------------------------------------------------------------------------
// AC-M02: det of identity is 1
// Given: m = identity()
// When:  det(m)
// Then:  result == 1.0f
// ---------------------------------------------------------------------------
TEST_CASE("AC-M02: det of identity matrix is 1", "[core][matrix3]") {
    constexpr Mat3  m = identity();
    constexpr float d = det(m);
    REQUIRE(d == Catch::Approx(1.0f).epsilon(kEps));
}

// ---------------------------------------------------------------------------
// AC-M03: det of a 90-degree rotation about Z is 1
// Given: R_z90 = rotation by 90° about Z-axis
//        column-major:  col[0]={0,1,0}, col[1]={-1,0,0}, col[2]={0,0,1}
// When:  det(R_z90)
// Then:  result == 1.0f  (rotation matrices have det = +1)
// ---------------------------------------------------------------------------
TEST_CASE("AC-M03: det of 90-degree Z-rotation is 1", "[core][matrix3]") {
    // Rotation by +90° about Z: x→y, y→-x, z→z
    // col[0] (image of x_hat) = { 0, 1, 0}
    // col[1] (image of y_hat) = {-1, 0, 0}
    // col[2] (image of z_hat) = { 0, 0, 1}
    constexpr Mat3 Rz90 {
        Vec3{ 0.0f,  1.0f, 0.0f},
        Vec3{-1.0f,  0.0f, 0.0f},
        Vec3{ 0.0f,  0.0f, 1.0f}
    };
    constexpr float d = det(Rz90);
    REQUIRE(d == Catch::Approx(1.0f).epsilon(kEps));
}

// ---------------------------------------------------------------------------
// AC-M04: multiply(matrix, vector) - 90-degree rotation maps x_hat to y_hat
// Given: R_z90 as above, v = x_hat = {1,0,0}
// When:  multiply(R_z90, v)
// Then:  result == {0,1,0}  (y_hat)
// ---------------------------------------------------------------------------
TEST_CASE("AC-M04: multiply rotation matrix by x_hat gives rotated vector", "[core][matrix3]") {
    constexpr Mat3 Rz90 {
        Vec3{ 0.0f,  1.0f, 0.0f},
        Vec3{-1.0f,  0.0f, 0.0f},
        Vec3{ 0.0f,  0.0f, 1.0f}
    };
    constexpr Vec3 x_hat{1.0f, 0.0f, 0.0f};
    constexpr Vec3 result = multiply(Rz90, x_hat);
    REQUIRE(approx_equal(result, Vec3{0.0f, 1.0f, 0.0f}, kEps));
}

// ---------------------------------------------------------------------------
// AC-M05: rotation matrix preserves vector length
// Given: R_z90 as above, v = {3,4,0}  (magnitude 5)
// When:  multiply(R_z90, v)
// Then:  magnitude of result == 5.0f
// ---------------------------------------------------------------------------
TEST_CASE("AC-M05: rotation matrix preserves vector length", "[core][matrix3]") {
    constexpr Mat3 Rz90 {
        Vec3{ 0.0f,  1.0f, 0.0f},
        Vec3{-1.0f,  0.0f, 0.0f},
        Vec3{ 0.0f,  0.0f, 1.0f}
    };
    const Vec3  v      = {3.0f, 4.0f, 0.0f};
    const Vec3  result = multiply(Rz90, v);
    // magnitude(v) == 5.0f; rotation must preserve it
    const float len    = magnitude(result);
    REQUIRE(len == Catch::Approx(5.0f).epsilon(kEps));
}

// ---------------------------------------------------------------------------
// AC-M06: multiply(identity, v) == v for arbitrary vector
// Given: I = identity(), v = {7, -3, 0.5}
// When:  multiply(I, v)
// Then:  result == v
// ---------------------------------------------------------------------------
TEST_CASE("AC-M06: multiplying identity matrix by a vector returns the vector", "[core][matrix3]") {
    constexpr Mat3 I  = identity();
    constexpr Vec3 v  = {7.0f, -3.0f, 0.5f};
    constexpr Vec3 result = multiply(I, v);
    REQUIRE(approx_equal(result, v, kEps));
}

// ---------------------------------------------------------------------------
// AC-M07: multiply(identity, identity) == identity
// Given: I = identity()
// When:  multiply(I, I)
// Then:  result == identity()
// ---------------------------------------------------------------------------
TEST_CASE("AC-M07: identity composed with identity is identity", "[core][matrix3]") {
    constexpr Mat3 I      = identity();
    constexpr Mat3 result = multiply(I, I);
    REQUIRE(mat_approx_equal(result, I));
}

// ---------------------------------------------------------------------------
// AC-M08: transpose of a rotation matrix equals its inverse
// Given: R_z90 as above
// When:  Rt = transpose(R_z90)
// Then:  multiply(R_z90, Rt) == identity()  (R * R^T = I for rotation)
// ---------------------------------------------------------------------------
TEST_CASE("AC-M08: transpose of rotation matrix is its inverse", "[core][matrix3]") {
    constexpr Mat3 Rz90 {
        Vec3{ 0.0f,  1.0f, 0.0f},
        Vec3{-1.0f,  0.0f, 0.0f},
        Vec3{ 0.0f,  0.0f, 1.0f}
    };
    constexpr Mat3 Rt     = transpose(Rz90);
    constexpr Mat3 result = multiply(Rz90, Rt);
    constexpr Mat3 I      = identity();
    REQUIRE(mat_approx_equal(result, I, kEpsMed));
}

// ---------------------------------------------------------------------------
// AC-M09: at() accessor reflects column-major storage correctly
// Given: m constructed with known column vectors
// When:  at(m, row, col) is called for every (row, col)
// Then:  values match the expected column-major layout
// ---------------------------------------------------------------------------
TEST_CASE("AC-M09: at() accessor reflects column-major storage", "[core][matrix3]") {
    // Build a matrix with recognizable values so misrouted indices are obvious.
    // col[0] = {11,21,31}, col[1] = {12,22,32}, col[2] = {13,23,33}
    constexpr Mat3 m {
        Vec3{11.0f, 21.0f, 31.0f},
        Vec3{12.0f, 22.0f, 32.0f},
        Vec3{13.0f, 23.0f, 33.0f}
    };
    // Row 0 (x components of each column)
    REQUIRE(at(m, 0, 0) == 11.0f);
    REQUIRE(at(m, 0, 1) == 12.0f);
    REQUIRE(at(m, 0, 2) == 13.0f);
    // Row 1 (y components)
    REQUIRE(at(m, 1, 0) == 21.0f);
    REQUIRE(at(m, 1, 1) == 22.0f);
    REQUIRE(at(m, 1, 2) == 23.0f);
    // Row 2 (z components)
    REQUIRE(at(m, 2, 0) == 31.0f);
    REQUIRE(at(m, 2, 1) == 32.0f);
    REQUIRE(at(m, 2, 2) == 33.0f);
}

// ---------------------------------------------------------------------------
// AC-M10: det of composed rotation == 1  (composition preserves det)
// Given: R_z90 as above, R2 = some other valid rotation (180° about Z)
//        col[0]={-1,0,0}, col[1]={0,-1,0}, col[2]={0,0,1}
// When:  R_composed = multiply(R_z90, R2);  det(R_composed)
// Then:  det(R_composed) == 1.0f
// ---------------------------------------------------------------------------
TEST_CASE("AC-M10: composition of two rotation matrices has determinant 1", "[core][matrix3]") {
    constexpr Mat3 Rz90 {
        Vec3{ 0.0f,  1.0f, 0.0f},
        Vec3{-1.0f,  0.0f, 0.0f},
        Vec3{ 0.0f,  0.0f, 1.0f}
    };
    // Rotation by 180° about Z: x→-x, y→-y, z→z
    constexpr Mat3 Rz180 {
        Vec3{-1.0f,  0.0f, 0.0f},
        Vec3{ 0.0f, -1.0f, 0.0f},
        Vec3{ 0.0f,  0.0f, 1.0f}
    };
    constexpr Mat3  R_comp = multiply(Rz90, Rz180);
    constexpr float d      = det(R_comp);
    REQUIRE(d == Catch::Approx(1.0f).epsilon(kEps));
}

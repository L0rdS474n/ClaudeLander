// tests/test_vec3.cpp - Pass 1 Vec3 math layer tests.
//
// Covers AC-V01 through AC-V11.
// All tests are deterministic: no clocks, no filesystem, no random_device.
// All float comparisons use explicit epsilon (1e-5f) or Catch::Approx.

#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>

#include "core/vec3.hpp"

// ---------------------------------------------------------------------------
// Local epsilon - tighter than the default_state Approx epsilon.
// ---------------------------------------------------------------------------
static constexpr float kEps = 1e-5f;

// ---------------------------------------------------------------------------
// AC-V01: dot product of small integers
// Given: a = {1,2,3}, b = {4,5,6}
// When:  dot(a, b) is computed
// Then:  result == 1*4 + 2*5 + 3*6 == 32
// ---------------------------------------------------------------------------
TEST_CASE("AC-V01: dot product of small integers", "[core][vec3]") {
    constexpr Vec3 a{1.0f, 2.0f, 3.0f};
    constexpr Vec3 b{4.0f, 5.0f, 6.0f};
    constexpr float result = dot(a, b);
    REQUIRE(result == 32.0f);
}

// ---------------------------------------------------------------------------
// AC-V02: dot product of perpendicular unit vectors is zero
// Given: x_hat = {1,0,0}, y_hat = {0,1,0}
// When:  dot(x_hat, y_hat) is computed
// Then:  result == 0.0
// ---------------------------------------------------------------------------
TEST_CASE("AC-V02: dot product of perpendicular unit vectors is zero", "[core][vec3]") {
    constexpr Vec3 x_hat{1.0f, 0.0f, 0.0f};
    constexpr Vec3 y_hat{0.0f, 1.0f, 0.0f};
    constexpr float result = dot(x_hat, y_hat);
    REQUIRE(result == 0.0f);
}

// ---------------------------------------------------------------------------
// AC-V03: cross product of x_hat and y_hat gives z_hat
// Given: x_hat = {1,0,0}, y_hat = {0,1,0}
// When:  cross(x_hat, y_hat)
// Then:  result == {0,0,1}
// ---------------------------------------------------------------------------
TEST_CASE("AC-V03: cross product of x_hat X y_hat equals z_hat", "[core][vec3]") {
    constexpr Vec3 x_hat{1.0f, 0.0f, 0.0f};
    constexpr Vec3 y_hat{0.0f, 1.0f, 0.0f};
    constexpr Vec3 result = cross(x_hat, y_hat);
    REQUIRE(approx_equal(result, Vec3{0.0f, 0.0f, 1.0f}, kEps));
}

// ---------------------------------------------------------------------------
// AC-V04: cross product is anti-commutative
// Given: a = {1,0,0}, b = {0,1,0}
// When:  cross(a,b) and cross(b,a) are computed
// Then:  cross(b,a) == -cross(a,b)
// ---------------------------------------------------------------------------
TEST_CASE("AC-V04: cross product is anti-commutative", "[core][vec3]") {
    constexpr Vec3 a{1.0f, 0.0f, 0.0f};
    constexpr Vec3 b{0.0f, 1.0f, 0.0f};
    constexpr Vec3 ab = cross(a, b);
    constexpr Vec3 ba = cross(b, a);
    REQUIRE(approx_equal(ba, -ab, kEps));
}

// ---------------------------------------------------------------------------
// AC-V05: magnitude of a known vector
// Given: v = {3,4,0}
// When:  magnitude(v)
// Then:  result == 5.0f  (3-4-5 right triangle)
// ---------------------------------------------------------------------------
TEST_CASE("AC-V05: magnitude of a 3-4-5 vector", "[core][vec3]") {
    const Vec3 v{3.0f, 4.0f, 0.0f};
    const float result = magnitude(v);
    REQUIRE(result == Catch::Approx(5.0f).epsilon(kEps));
}

// ---------------------------------------------------------------------------
// AC-V06: normalize of a zero vector returns {0,0,0}
// Given: zero = {0,0,0}
// When:  normalize(zero)
// Then:  result == {0,0,0}  (no NaN, no Inf)
// ---------------------------------------------------------------------------
TEST_CASE("AC-V06: normalize of zero-vector returns zero-vector", "[core][vec3]") {
    const Vec3 zero{0.0f, 0.0f, 0.0f};
    const Vec3 result = normalize(zero);
    // Must not be NaN or Inf; must equal {0,0,0}
    REQUIRE(result.x == 0.0f);
    REQUIRE(result.y == 0.0f);
    REQUIRE(result.z == 0.0f);
}

// ---------------------------------------------------------------------------
// AC-V07: normalize of a non-zero vector has unit magnitude
// Given: v = {1,2,3}
// When:  normalize(v)
// Then:  magnitude(normalize(v)) == 1.0f within eps
// ---------------------------------------------------------------------------
TEST_CASE("AC-V07: normalize of non-zero vector produces unit length", "[core][vec3]") {
    const Vec3 v{1.0f, 2.0f, 3.0f};
    const Vec3 n = normalize(v);
    const float mag = magnitude(n);
    REQUIRE(mag == Catch::Approx(1.0f).epsilon(kEps));
}

// ---------------------------------------------------------------------------
// AC-V08: addition and subtraction are inverse operations
// Given: a = {1,2,3}, b = {4,5,6}
// When:  (a + b) - b
// Then:  result == a
// ---------------------------------------------------------------------------
TEST_CASE("AC-V08: add then subtract is identity", "[core][vec3]") {
    constexpr Vec3 a{1.0f, 2.0f, 3.0f};
    constexpr Vec3 b{4.0f, 5.0f, 6.0f};
    constexpr Vec3 result = (a + b) - b;
    REQUIRE(approx_equal(result, a, kEps));
}

// ---------------------------------------------------------------------------
// AC-V09: scalar multiplication - both orderings agree
// Given: v = {1,2,3}, s = 3.0f
// When:  v*s and s*v
// Then:  both produce {3,6,9}
// ---------------------------------------------------------------------------
TEST_CASE("AC-V09: scalar multiplication is commutative in ordering", "[core][vec3]") {
    constexpr Vec3  v{1.0f, 2.0f, 3.0f};
    constexpr float s = 3.0f;
    constexpr Vec3  vs = v * s;
    constexpr Vec3  sv = s * v;
    constexpr Vec3  expected{3.0f, 6.0f, 9.0f};
    REQUIRE(approx_equal(vs, expected, kEps));
    REQUIRE(approx_equal(sv, expected, kEps));
}

// ---------------------------------------------------------------------------
// AC-V10: cross product of parallel vectors is zero
// Given: a = {1,2,3}, b = 2*a = {2,4,6}
// When:  cross(a, b)
// Then:  result == {0,0,0}
// ---------------------------------------------------------------------------
TEST_CASE("AC-V10: cross product of parallel vectors is zero", "[core][vec3]") {
    constexpr Vec3 a{1.0f, 2.0f, 3.0f};
    constexpr Vec3 b{2.0f, 4.0f, 6.0f};
    constexpr Vec3 result = cross(a, b);
    REQUIRE(approx_equal(result, Vec3{0.0f, 0.0f, 0.0f}, kEps));
}

// ---------------------------------------------------------------------------
// AC-V11: scalar triple product - cross(a,b) is orthogonal to both a and b
// Given: a = {1,0,0}, b = {0,1,0}
// When:  c = cross(a,b) = {0,0,1}
// Then:  dot(a,c) == 0  and  dot(b,c) == 0
// (Verifies the triple-product orthogonality property)
// ---------------------------------------------------------------------------
TEST_CASE("AC-V11: scalar triple product orthogonality", "[core][vec3]") {
    constexpr Vec3 a{1.0f, 0.0f, 0.0f};
    constexpr Vec3 b{0.0f, 1.0f, 0.0f};
    constexpr Vec3 c = cross(a, b);
    constexpr float dot_ac = dot(a, c);
    constexpr float dot_bc = dot(b, c);
    REQUIRE(std::abs(dot_ac) <= kEps);
    REQUIRE(std::abs(dot_bc) <= kEps);
}

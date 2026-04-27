// src/core/vec3.hpp — Pass 1 stub header (declarations + constexpr definitions).
//
// Non-constexpr functions (magnitude, normalize) are declared here but
// defined in vec3.cpp (Implementation Gate 4).  All operators and helpers
// that are constexpr are defined inline here, as required by the C++ standard
// for constexpr to be usable in constant-expression contexts.
//
// Column convention: Y-DOWN world space (see docs/ARCHITECTURE.md).

#pragma once

#include <cmath>    // std::sqrt — used only in non-constexpr definitions
#include <cfloat>   // FLT_EPSILON

// ---------------------------------------------------------------------------
// Vec3 struct
// ---------------------------------------------------------------------------

struct Vec3 {
    float x, y, z;
};

// ---------------------------------------------------------------------------
// Arithmetic operators — constexpr, defined inline (required by standard)
// ---------------------------------------------------------------------------

constexpr Vec3 operator+(Vec3 a, Vec3 b) noexcept {
    return { a.x + b.x, a.y + b.y, a.z + b.z };
}

constexpr Vec3 operator-(Vec3 a, Vec3 b) noexcept {
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

constexpr Vec3 operator-(Vec3 a) noexcept {
    return { -a.x, -a.y, -a.z };
}

constexpr Vec3 operator*(Vec3 v, float s) noexcept {
    return { v.x * s, v.y * s, v.z * s };
}

constexpr Vec3 operator*(float s, Vec3 v) noexcept {
    return { s * v.x, s * v.y, s * v.z };
}

// ---------------------------------------------------------------------------
// Geometric functions — constexpr where standard permits, inline definitions
// ---------------------------------------------------------------------------

constexpr float dot(Vec3 a, Vec3 b) noexcept {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

constexpr Vec3 cross(Vec3 a, Vec3 b) noexcept {
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

// magnitude and normalize are NOT constexpr (std::sqrt is not constexpr in
// C++20 in all implementations for runtime use).  Declarations only; .cpp
// provides the definitions.
float magnitude(Vec3 v) noexcept;
Vec3  normalize(Vec3 v) noexcept;   // returns {0,0,0} for zero-vector input

// ---------------------------------------------------------------------------
// Approximate equality helper
// ---------------------------------------------------------------------------

constexpr bool approx_equal(Vec3 a, Vec3 b, float eps = 1e-5f) noexcept {
    auto abs_f = [](float f) constexpr -> float { return f < 0.0f ? -f : f; };
    return abs_f(a.x - b.x) <= eps &&
           abs_f(a.y - b.y) <= eps &&
           abs_f(a.z - b.z) <= eps;
}

// src/core/vec3.cpp -- Pass 1 Vec3 non-constexpr definitions.
//
// magnitude() and normalize() live here because std::sqrt is not usable in a
// constant-expression context for runtime values.  The constexpr operators
// and dot/cross helpers are defined inline in vec3.hpp.
//
// Zero-vector contract (AC-V05/AC-V06):
//   normalize({0,0,0}) returns {0,0,0} -- never NaN, never Inf.
//   The threshold for "zero" is magnitude < 1e-30f, which is well below any
//   numerically meaningful float magnitude and well above the denormal floor.

#include "core/vec3.hpp"

#include <cmath>   // std::sqrt

float magnitude(Vec3 v) noexcept {
    // dot(v, v) is constexpr in the header; std::sqrt is the runtime step.
    return std::sqrt(dot(v, v));
}

Vec3 normalize(Vec3 v) noexcept {
    const float mag = magnitude(v);

    // Zero-vector contract: return {0,0,0} rather than dividing by zero.
    // The 1e-30f threshold is intentionally far below any vector magnitude
    // the simulation will encounter; it exists purely to guard the divide.
    if (mag < 1.0e-30f) {
        return Vec3{0.0f, 0.0f, 0.0f};
    }

    const float inv = 1.0f / mag;
    return v * inv;
}

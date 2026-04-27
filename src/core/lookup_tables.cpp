// src/core/lookup_tables.cpp -- Pass 1 fixed-point look-up tables and the
// float wrappers that read from them.
//
// Tables are generated once at static initialization time using libm
// (std::sin, std::atan, std::sqrt) so we get host-quality reference values
// rather than carrying thousands of hand-converted constants in the source.
// AC-L10's tolerance of 1e-3 absorbs any cross-platform libm variance.
//
// Q31 convention:
//   INT32_MAX  represents +1.0
//   0          represents  0.0
//   -INT32_MAX represents -1.0  (we use -INT32_MAX, NOT INT32_MIN, so
//                                positive and negative ranges are symmetric)
//
// All four float wrappers are the single conversion boundary between the
// integer table storage and the float consumers per ADR-0002.

#include "core/lookup_tables.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

constexpr double kPiDouble = 3.14159265358979323846;
constexpr float  kPiFloat  = 3.14159265358979323846f;
constexpr float  kTwoPi    = kPiFloat * 2.0f;
constexpr float  kHalfPi   = kPiFloat * 0.5f;

// Q31 scale: INT32_MAX represents 1.0.  We treat it as a positive double
// during table generation so the rounding behaviour is symmetric.
constexpr double kQ31Scale = 2147483647.0;   // = 0x7FFFFFFF

// Wrap an angle into [0, 2*pi).
inline float wrap_radians(float radians) noexcept {
    // std::fmod returns a value with the same sign as the dividend, so we
    // shift negatives into the positive period before returning.
    float wrapped = std::fmod(radians, kTwoPi);
    if (wrapped < 0.0f) wrapped += kTwoPi;
    return wrapped;
}

}  // namespace

// ---------------------------------------------------------------------------
// Table definitions -- static initialization via immediately-invoked lambdas.
// ---------------------------------------------------------------------------

namespace tables {

const std::array<std::int32_t, 1024> sin_q31 = []{
    std::array<std::int32_t, 1024> t{};
    for (std::size_t i = 0; i < 1024; ++i) {
        const double angle = 2.0 * kPiDouble * static_cast<double>(i) / 1024.0;
        const double v = std::sin(angle);
        t[i] = static_cast<std::int32_t>(v * kQ31Scale);
    }
    return t;
}();

const std::array<std::int32_t, 128> arctan_q31 = []{
    std::array<std::int32_t, 128> t{};
    // Sampling-point choice (recorded in ADR-0002 LFSR/arctan-sampling):
    // We sample at i/127 rather than i/128 so that the table's last entry
    // (index 127) represents arctan(1.0) = pi/4 exactly.  This satisfies
    // both AC-L17 (arctan_lut(1.0f) ~= pi/4 within 5e-3) and AC-L20
    // (arctan_lut(127/128) ~= atan(127/128) within 5e-3).  Sampling at
    // i/128 would leave the wrapper unable to return a value close enough
    // to pi/4 on input 1.0, since the index would saturate at 127 covering
    // arctan(127/128) ~= 0.7768 (off from pi/4 ~= 0.7854 by ~9e-3).
    for (std::size_t i = 0; i < 128; ++i) {
        const double v = std::atan(static_cast<double>(i) / 127.0);
        // Wrapper formula: result = table[i] / INT32_MAX * pi
        // Therefore table[i] = (INT32_MAX / pi) * arctan(i/127).
        t[i] = static_cast<std::int32_t>((kQ31Scale / kPiDouble) * v);
    }
    return t;
}();

const std::array<std::int32_t, 1024> sqrt_q31 = []{
    std::array<std::int32_t, 1024> t{};
    for (std::size_t i = 0; i < 1024; ++i) {
        // sqrt(x) over x in [0, 1), sampled at i/1024.
        const double v = std::sqrt(static_cast<double>(i) / 1024.0);
        t[i] = static_cast<std::int32_t>(kQ31Scale * v);
    }
    return t;
}();

}  // namespace tables

// ---------------------------------------------------------------------------
// Float wrappers
// ---------------------------------------------------------------------------

float sin_lut(float radians) noexcept {
    const float wrapped = wrap_radians(radians);
    // Map [0, 2pi) to [0, 1024) and mask down with & 1023 so any rounding
    // that nudges to 1024 wraps back to 0 cleanly.
    const int idx = static_cast<int>(wrapped * (1024.0f / kTwoPi)) & 1023;
    return static_cast<float>(tables::sin_q31[static_cast<std::size_t>(idx)])
         / static_cast<float>(kQ31Scale);
}

float cos_lut(float radians) noexcept {
    return sin_lut(radians + kHalfPi);
}

float sqrt_lut(float x) noexcept {
    // Outside the table's [0, 1] domain, fall back to libm.  This keeps
    // callers safe if they ever pass values just over 1.0f due to rounding.
    if (x < 0.0f || x > 1.0f) {
        return std::sqrt(x);
    }
    int idx = static_cast<int>(x * 1024.0f);
    if (idx < 0)        idx = 0;
    else if (idx > 1023) idx = 1023;
    return static_cast<float>(tables::sqrt_q31[static_cast<std::size_t>(idx)])
         / static_cast<float>(kQ31Scale);
}

float arctan_lut(float ratio) noexcept {
    // Outside [0, 1] we cannot use the first-octant table directly; defer to
    // libm.  Inside the table's domain we apply the inverse of the table's
    // generation formula:  result = table[idx] / INT32_MAX * pi
    // Index mapping pairs with the i/127 sampling choice (see arctan_q31
    // above): ratio*127 makes ratio=1.0 land exactly at idx=127, which
    // stores arctan(1) = pi/4.
    if (ratio < 0.0f || ratio > 1.0f) {
        return std::atan(ratio);
    }
    int idx = static_cast<int>(ratio * 127.0f);
    if (idx < 0)        idx = 0;
    else if (idx > 127) idx = 127;
    return static_cast<float>(tables::arctan_q31[static_cast<std::size_t>(idx)])
         / static_cast<float>(kQ31Scale)
         * kPiFloat;
}

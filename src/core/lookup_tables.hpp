// src/core/lookup_tables.hpp — Pass 1 stub header.
//
// Three fixed-point look-up tables (Q3.31 format, i.e. values scaled by
// 2^31) and four float-returning wrapper functions.
//
// Table contents are defined in lookup_tables.cpp (Implementation Gate 4).
// This header only declares the extern arrays and function prototypes.
//
// Q31 convention: INT32_MAX  ≈  1.0
//                 0          ≈  0.0
//                 INT32_MIN  ≈ -1.0  (saturated; not normally reached)
//
// sin_q31:    1024 entries covering one full 2π period (index 0..1023).
//             Index 256 ≈ sin(π/2) = 1  → INT32_MAX
//             Index 512 ≈ sin(π)   = 0
//             Index 768 ≈ sin(3π/2)= −1 → −INT32_MAX (note: not INT32_MIN)
//
// arctan_q31: 128 entries; maps ratio ∈ [0, 1] linearly to arctan result.
//             Index 0  → 0  (arctan(0)  = 0)
//             Index 127 → arctan(127/128) scaled to Q31 of radians / (π/4)
//             Strictly increasing.
//
// sqrt_q31:   1024 entries; maps x ∈ [0, 1] → √x in Q31.
//             Index 0 → 0  (√0 = 0)
//             Monotonically non-decreasing.

#pragma once

#include <array>
#include <cstdint>

// ---------------------------------------------------------------------------
// Raw Q31 tables (definitions in lookup_tables.cpp)
// ---------------------------------------------------------------------------

namespace tables {
    extern const std::array<std::int32_t, 1024> sin_q31;
    extern const std::array<std::int32_t, 128>  arctan_q31;
    extern const std::array<std::int32_t, 1024> sqrt_q31;
}

// ---------------------------------------------------------------------------
// Float-returning wrappers (definitions in lookup_tables.cpp)
// ---------------------------------------------------------------------------

// sin_lut: full-range sine via table interpolation.
// Input: radians (any value; wrapped internally to [0, 2π)).
// Output: approximately sin(radians), error ≤ 5e-3 vs std::sin.
float sin_lut(float radians) noexcept;

// cos_lut: implemented as sin_lut(radians + π/2).
float cos_lut(float radians) noexcept;

// sqrt_lut: square root via table.
// Input: x ∈ [0, 1].  Behaviour for x > 1 is implementation-defined.
// Output: approximately √x.
float sqrt_lut(float x) noexcept;

// arctan_lut: arctangent via table.
// Input: ratio ∈ [0, 1]  (opposite/adjacent for first-octant angles).
// Output: approximately arctan(ratio) in radians.
float arctan_lut(float ratio) noexcept;

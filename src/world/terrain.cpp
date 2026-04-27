// src/world/terrain.cpp -- Pass 2 procedural terrain implementation.
//
// Six-term Fourier altitude formula (see docs/research/pass-2-terrain-spec.md
// and docs/plans/pass-2-terrain.md):
//
//   altitude(x, z) = LAND_MID_HEIGHT - sum / 256
//
//   sum =   2 * sin(( 1*xi -  2*zi) * 2pi / 1024)
//         + 2 * sin(( 4*xi +  3*zi) * 2pi / 1024)
//         + 2 * sin((-5*xi +  3*zi) * 2pi / 1024)
//         + 2 * sin(( 7*xi +  5*zi) * 2pi / 1024)
//         + 1 * sin(( 5*xi + 11*zi) * 2pi / 1024)
//         + 1 * sin((10*xi +  7*zi) * 2pi / 1024)
//
// where xi = floor(x), zi = floor(z).  We do not call std::sin: the sin LUT
// from src/core/lookup_tables.cpp is read directly with integer index, which
// makes the sum deterministic and exactly periodic with period 1024 in each
// axis (planner D2, AC-W07..W09).
//
// LUT scaling: tables::sin_q31[idx] is in Q31 (INT32_MAX represents +1.0).
// Convert to a float in [-1, +1] via division by float(INT32_MAX).  The
// conversion happens here, on the float side of the boundary; the caller
// never sees an int32_t (single-flip-point rule, ADR-0002).

#include <array>
#include <cmath>
#include <cstdint>

#include "core/vec3.hpp"
#include "core/lookup_tables.hpp"
#include "world/terrain.hpp"

namespace terrain {

namespace {

// Convert Q31 to float in [-1, +1].  INT32_MAX == 1.0; -INT32_MAX == -1.0.
constexpr float kQ31InverseScale = 1.0f / static_cast<float>(INT32_MAX);

// Read the sin LUT at integer argument arg, applying the modulo-1024 rule
// recommended by the spec:
//
//     idx = ((arg % 1024) + 1024) % 1024
//
// Two-step modulo guarantees a non-negative index even when arg is negative
// (C++ integer % can return a negative remainder for negative operands).
inline float sin_lut_scaled(std::int32_t arg) noexcept {
    constexpr std::int32_t kPeriod = 1024;
    const std::int32_t idx = ((arg % kPeriod) + kPeriod) % kPeriod;
    return static_cast<float>(tables::sin_q31[static_cast<std::size_t>(idx)])
           * kQ31InverseScale;
}

// Floor a float toward -infinity into an int32_t.  Planner KOQ-3 locks the
// boundary coercion to std::floor so altitude(0.5f, ...) == altitude(0.0f, ...).
//
// Iter-5 fence: NaN and infinities short-circuit to 0 (the LAND_MID_HEIGHT
// origin) so a NaN-poisoned input cannot reach `static_cast<int32_t>(NaN)`,
// which is undefined behaviour per [conv.fpint].  Returning 0 yields
// altitude(0, 0) == LAND_MID_HEIGHT downstream -- a predictable, finite
// fallback rather than UB.
inline std::int32_t floor_to_int(float v) noexcept {
    if (!std::isfinite(v)) {
        return 0;
    }
    return static_cast<std::int32_t>(std::floor(v));
}

}  // namespace

namespace detail {

float raw_sum_at(float x, float z) noexcept {
    const std::int32_t xi = floor_to_int(x);
    const std::int32_t zi = floor_to_int(z);

    // Six-term Fourier sum.  Term order is canonical (matches the spec table)
    // so the float-arithmetic ordering of additions is fixed -- determinism
    // requirement AC-W01..W06.
    //
    // Amplitudes: terms 1..4 are doubled; terms 5..6 are unit.
    //
    // Note: amplitudes are folded into the multiplication on the float side.
    // We could pre-multiply the LUT read by 2 inside an int domain, but doing
    // it as float keeps the implementation readable and the precision loss is
    // negligible (single-bit Q31 quantisation feeds float multiplications).
    const float t1 = 2.0f * sin_lut_scaled( 1 * xi -  2 * zi);
    const float t2 = 2.0f * sin_lut_scaled( 4 * xi +  3 * zi);
    const float t3 = 2.0f * sin_lut_scaled(-5 * xi +  3 * zi);
    const float t4 = 2.0f * sin_lut_scaled( 7 * xi +  5 * zi);
    const float t5 =        sin_lut_scaled( 5 * xi + 11 * zi);
    const float t6 =        sin_lut_scaled(10 * xi +  7 * zi);

    return t1 + t2 + t3 + t4 + t5 + t6;
}

}  // namespace detail

float altitude(float x, float z) noexcept {
    return LAND_MID_HEIGHT - detail::raw_sum_at(x, z) / 256.0f;
}

std::array<Vec3, MESH_VERTEX_COUNT>
build_mesh(float centre_x, float centre_z) noexcept {
    std::array<Vec3, MESH_VERTEX_COUNT> mesh{};

    // Mesh anchor: snap the centre vertex to the integer-tile lattice beneath
    // the ship (planner D3).  The centre vertex sits at index 5*13 + 6 = 71;
    // rows extend [-5, +5] in z; columns extend [-6, +6] in x.
    const float anchor_x = std::floor(centre_x);
    const float anchor_z = std::floor(centre_z);

    // Half-extents.  MESH_COLS is 13 (odd) so col offsets are [-6, +6];
    // MESH_ROWS is 11 (odd) so row offsets are [-5, +5].
    constexpr int kColHalf = static_cast<int>(MESH_COLS) / 2;  // = 6
    constexpr int kRowHalf = static_cast<int>(MESH_ROWS) / 2;  // = 5

    for (std::size_t row = 0; row < MESH_ROWS; ++row) {
        const int row_off = static_cast<int>(row) - kRowHalf;
        const float vz = anchor_z + static_cast<float>(row_off) * TILE_SIZE;
        for (std::size_t col = 0; col < MESH_COLS; ++col) {
            const int col_off = static_cast<int>(col) - kColHalf;
            const float vx = anchor_x + static_cast<float>(col_off) * TILE_SIZE;
            const float vy = altitude(vx, vz);
            mesh[row * MESH_COLS + col] = Vec3{ vx, vy, vz };
        }
    }

    return mesh;
}

}  // namespace terrain

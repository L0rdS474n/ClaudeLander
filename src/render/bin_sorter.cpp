// src/render/bin_sorter.cpp -- Pass 8 bin-sort depth ordering (clean-room).
//
// Most of BinSorter<Payload> lives in the header (template).  Only the
// non-template free function bin_for_z is defined here; this keeps the
// .cpp small and gives the linker a single TU to attribute the symbol to.
//
// Boundaries (AC-B80): no raylib, no world/, no entities/, no <random>,
// no <chrono>.  Pure float arithmetic and a single integer cast.

#include "render/bin_sorter.hpp"

#include <cstddef>
#include <optional>

namespace render {

// ---------------------------------------------------------------------------
// bin_for_z -- documented in render/bin_sorter.hpp.
// ---------------------------------------------------------------------------
//
// Implementation notes:
//
//   1. Compute delta = landscape_z - z as float.  No fabs, no rounding;
//      truncation toward zero on the cast matches the ARM integer
//      subtraction (D-BinCount).
//
//   2. Range check the float BEFORE casting.  Casting a negative float
//      to std::size_t is undefined behaviour in C++; casting a value
//      >= 2^64 is also undefined.  We gate both with an explicit check
//      against the float-domain bounds [0, kBinCount) so the cast that
//      follows is always defined.
//
//   3. The bound is half-open: delta in [0, kBinCount) maps to bins
//      0..kBinCount-1.  AC-B02 (delta = 10.0) and AC-B04 (delta = 11.0)
//      pin the boundary on each side.

std::optional<std::size_t> bin_for_z(float landscape_z, float z) noexcept {
    const float delta = landscape_z - z;

    // Reject negative bins (z is closer than any landscape row) and
    // bins >= kBinCount (z is further than the horizon).  AC-B03 / AC-B04.
    if (delta < 0.0f) {
        return std::nullopt;
    }
    if (delta >= static_cast<float>(kBinCount)) {
        return std::nullopt;
    }

    // Truncation toward zero.  delta is non-negative here, so the cast
    // is well-defined and matches the integer-subtraction semantics.
    return static_cast<std::size_t>(delta);
}

}  // namespace render

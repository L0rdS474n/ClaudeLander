// src/core/prng.cpp -- Pass 1 LFSR PRNG implementation.
//
// 33-bit linear-feedback shift register modelled on the original ARM2 Lander
// PRNG described in the Acorn ARM Assembler Manual section 11.2 (page 96)
// and visible in the bbcelite Lander disassembly.
//
// Bit-layout interpretation (recorded in ADR-0002 PRNG-vectors):
//   The original LFSR is described as 33 bits wide with feedback taps "at
//   bits 20 and 33".  We map it onto two 32-bit words (seed1 high, seed2
//   low) such that:
//     - tap "at bit 20" reads from bit 19 of seed1
//     - tap "at bit 33" reads from bit 0 of seed2
//   The feedback bit is the XOR of those two taps.
//   Each call performs 32 single-bit shifts so a full 32-bit output emerges.
//
// This interpretation is internally consistent (deterministic, non-zero,
// reproducible) and satisfies AC-P02..P04, P06, P07.  AC-P05 (golden
// reference vectors against the original ROM) is deferred per ADR-0002:
// the test ships with [.golden] and is skipped by default until verified
// reference values are captured from a known-good source.

#include "core/prng.hpp"

#include <cstdint>

Pair next(LfsrState& state) noexcept {
    // 32 LFSR steps per call -- one per output bit shifted into r0/r1 across
    // the two-word virtual register.
    for (int i = 0; i < 32; ++i) {
        // Feedback = XOR of the two taps, isolated to a single bit.
        const std::uint32_t feedback =
            ((state.seed1 >> 19) ^ (state.seed2 >> 0)) & 1u;

        // Shift the 33-bit register left by one:
        //   - the new high word is old seed1 << 1, OR'd with the carry-out
        //     from seed2 (its top bit, bit 31).
        //   - the new low word is old seed2 << 1, OR'd with the freshly
        //     computed feedback bit at position 0.
        state.seed1 = (state.seed1 << 1) | (state.seed2 >> 31);
        state.seed2 = (state.seed2 << 1) | feedback;
    }

    return Pair{ state.seed1, state.seed2 };
}

float next_unit_float(LfsrState& state) noexcept {
    // Advance and consume r0 as the unit-float source.
    const Pair p = next(state);

    // Spec divisor (per ADR-0002): 2^32 = UINT32_MAX + 1.
    // Mathematically this maps r0/2^32 to [0, 1), but float-precision
    // rounding of r0 -> float can promote values near 0xFFFFFFFF to
    // exactly 4294967296.0f, which would yield exactly 1.0f and violate
    // AC-P04's strict upper bound.  We guard that single boundary case
    // by saturating to the largest float strictly less than 1.0f.
    constexpr float kDiv2Pow32 = 4294967296.0f;          // 2^32
    constexpr float kJustBelowOne = 0.99999994f;         // = nextafter(1.0f, 0.0f)

    const float ratio = static_cast<float>(p.r0) / kDiv2Pow32;
    return (ratio >= 1.0f) ? kJustBelowOne : ratio;
}

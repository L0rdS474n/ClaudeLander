// src/core/prng.hpp — Pass 1 stub header.
//
// Linear-feedback shift register (LFSR) PRNG modelled on the ARM2 Lander
// original.  Two 32-bit seed words advance independently; next() produces a
// Pair of 32-bit results and mutates the state in place.
//
// next() and next_unit_float() are NOT constexpr (they mutate state and may
// involve division).  Declarations only; .cpp provides definitions.

#pragma once

#include <cstdint>

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

struct LfsrState {
    std::uint32_t seed1;
    std::uint32_t seed2;
};

struct Pair {
    std::uint32_t r0;
    std::uint32_t r1;
};

// ---------------------------------------------------------------------------
// Seed
// ---------------------------------------------------------------------------

// Default seed values taken from the original Lander binary (bbcelite).
constexpr LfsrState default_state() noexcept {
    return { 0x4F9C3490u, 0xDA0383CFu };
}

// ---------------------------------------------------------------------------
// Advance functions — declared here, defined in prng.cpp
// ---------------------------------------------------------------------------

// Advance the LFSR by one step.  Returns the raw pair of output words.
// Mutates state in place.
Pair next(LfsrState& state) noexcept;

// Advance the LFSR and return a value in [0.0f, 1.0f).
float next_unit_float(LfsrState& state) noexcept;

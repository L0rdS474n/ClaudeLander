// tests/test_prng.cpp - Pass 1 LFSR PRNG tests.
//
// Covers AC-P01 through AC-P07.
// AC-P05 is tagged [.golden] and SKIPPED by default (see note below).
//
// Determinism plan:
//   Every test begins from default_state() or an explicit literal seed.
//   No system clock, no std::random_device, no filesystem access.
//
// AC-P05 strategy:
//   The expected output hex values for the reference LFSR sequence are NOT
//   KNOWN at this stage.  They must be captured from a verified reference:
//   either the original ARM2 disassembly (bbcelite), or an empirically-verified
//   ARM2 emulator run.  The test is structured to fail loudly if accidentally
//   enabled with placeholder constants (0xDEADBEEFu / 0xCAFEBABEu).
//   DO NOT GUESS OR FABRICATE these values.

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cstdint>
#include <cmath>

#include "core/prng.hpp"

// ---------------------------------------------------------------------------
// AC-P01: default_state() returns the documented seed values
// Given: documentation specifies seed1 = 0x4F9C3490u, seed2 = 0xDA0383CFu
// When:  default_state() is called
// Then:  state.seed1 == 0x4F9C3490u, state.seed2 == 0xDA0383CFu
// ---------------------------------------------------------------------------
TEST_CASE("AC-P01: default_state returns documented seed constants", "[core][prng]") {
    constexpr LfsrState s = default_state();
    REQUIRE(s.seed1 == 0x4F9C3490u);
    REQUIRE(s.seed2 == 0xDA0383CFu);
}

// ---------------------------------------------------------------------------
// AC-P02: next() mutates state (seeds change after one step)
// Given: state = default_state()
// When:  next(state) is called once
// Then:  state.seed1 or state.seed2 differs from the original values
// ---------------------------------------------------------------------------
TEST_CASE("AC-P02: next() mutates LFSR state", "[core][prng]") {
    LfsrState state = default_state();
    const std::uint32_t seed1_before = state.seed1;
    const std::uint32_t seed2_before = state.seed2;
    (void)next(state);
    // At least one seed word must have changed
    const bool changed = (state.seed1 != seed1_before) || (state.seed2 != seed2_before);
    REQUIRE(changed);
}

// ---------------------------------------------------------------------------
// AC-P03: next() returns a Pair with at least one non-zero output
// Given: state = default_state()
// When:  p = next(state)
// Then:  p.r0 != 0 || p.r1 != 0  (LFSR of non-zero seed never outputs all-zero)
// ---------------------------------------------------------------------------
TEST_CASE("AC-P03: next() returns non-zero output for non-zero seed", "[core][prng]") {
    LfsrState state = default_state();
    const Pair p = next(state);
    REQUIRE((p.r0 != 0u || p.r1 != 0u));
}

// ---------------------------------------------------------------------------
// AC-P04: next_unit_float() result is in [0.0f, 1.0f)
// Given: state = default_state()
// When:  next_unit_float(state) is called 1000 times
// Then:  every result r satisfies 0.0f <= r < 1.0f
// ---------------------------------------------------------------------------
TEST_CASE("AC-P04: next_unit_float always returns value in 0.0..1.0 exclusive", "[core][prng]") {
    LfsrState state = default_state();
    for (int i = 0; i < 1000; ++i) {
        const float r = next_unit_float(state);
        REQUIRE(r >= 0.0f);
        REQUIRE(r < 1.0f);
    }
}

// ---------------------------------------------------------------------------
// AC-P05: golden-sequence test - SKIPPED by default [.golden]
//
// This test verifies that next() produces a byte-exact match to the reference
// ARM2 LFSR sequence from the original Lander binary (bbcelite disassembly).
//
// STATUS: PLACEHOLDER - expected values are NOT yet known.
//
// FIXME(Gate-2-followup): expected values to be captured from reference
// disassembly or empirically-verified ARM2 emulator.  DO NOT GUESS.
// When real values are known, replace 0xDEADBEEFu / 0xCAFEBABEu below with
// the verified constants and remove the [.golden] skip tag if desired, or
// keep it as an opt-in regression guard.
//
// Given: state = default_state()
// When:  next(state) called 3 times (producing 3 Pairs)
// Then:  exact hex values match the ARM2 reference sequence
// ---------------------------------------------------------------------------
TEST_CASE("AC-P05: PRNG golden sequence matches ARM2 reference", "[core][prng][.golden]") {
    // PLACEHOLDER VALUES - these are NOT real expected outputs.
    // Replace with verified reference values before enabling this test.
    static constexpr std::uint32_t kExpectedR0_step1 = 0xDEADBEEFu;
    static constexpr std::uint32_t kExpectedR1_step1 = 0xCAFEBABEu;
    static constexpr std::uint32_t kExpectedR0_step2 = 0xDEADBEEFu;
    static constexpr std::uint32_t kExpectedR1_step2 = 0xCAFEBABEu;
    static constexpr std::uint32_t kExpectedR0_step3 = 0xDEADBEEFu;
    static constexpr std::uint32_t kExpectedR1_step3 = 0xCAFEBABEu;

    LfsrState state = default_state();

    const Pair p1 = next(state);
    REQUIRE(p1.r0 == kExpectedR0_step1);
    REQUIRE(p1.r1 == kExpectedR1_step1);

    const Pair p2 = next(state);
    REQUIRE(p2.r0 == kExpectedR0_step2);
    REQUIRE(p2.r1 == kExpectedR1_step2);

    const Pair p3 = next(state);
    REQUIRE(p3.r0 == kExpectedR0_step3);
    REQUIRE(p3.r1 == kExpectedR1_step3);
}

// ---------------------------------------------------------------------------
// AC-P06: sequence is reproducible from same seed
// Given: state_a = default_state(), state_b = default_state()
// When:  next() called N times on each independently
// Then:  each call produces identical Pair values
// ---------------------------------------------------------------------------
TEST_CASE("AC-P06: identical seeds produce identical sequences", "[core][prng]") {
    LfsrState state_a = default_state();
    LfsrState state_b = default_state();

    for (int i = 0; i < 100; ++i) {
        const Pair pa = next(state_a);
        const Pair pb = next(state_b);
        REQUIRE(pa.r0 == pb.r0);
        REQUIRE(pa.r1 == pb.r1);
    }
}

// ---------------------------------------------------------------------------
// AC-P07: different seeds produce different first outputs
// Given: state_def = default_state(), state_alt = {0x12345678u, 0x87654321u}
// When:  next() called once on each
// Then:  outputs differ  (distinct seeds must not collide on first step)
// ---------------------------------------------------------------------------
TEST_CASE("AC-P07: different seeds produce different output sequences", "[core][prng]") {
    LfsrState state_def = default_state();
    LfsrState state_alt = { 0x12345678u, 0x87654321u };

    const Pair p_def = next(state_def);
    const Pair p_alt = next(state_alt);

    // At least one of the two output words must differ
    const bool differ = (p_def.r0 != p_alt.r0) || (p_def.r1 != p_alt.r1);
    REQUIRE(differ);
}

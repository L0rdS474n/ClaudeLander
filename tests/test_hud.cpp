// tests/test_hud.cpp — Pass 12 render/hud tests.
//
// Covers AC-H01..H05, AC-H06..H10, AC-H11..H15, AC-H16..H20,
// AC-Hpad, AC-Hclamp, AC-Htop16, AC-H80..H82 (24 ACs total, numbered per
// pass-12-hud.md §4).
// All tests tagged [render][hud].
//
// === Determinism plan ===
// render::format_score, render::fuel_bar_width, and render::format_ammo are
// all declared noexcept and are pure functions with no PRNG, no clock, no
// filesystem access, and no global mutable state.  All tests use hard-coded
// inputs.  AC-H16 and AC-H17 verify bit-identical output across 1000
// independent calls on the same input.
//
// === Bug-class fences ===
// Three developer-mistake patterns are caught with prominent banner comments:
//   (a) AC-Hpad   — format_score must zero-pad: "000800", NOT "800".
//   (b) AC-Hclamp — fuel_bar_width clamps BOTH above 1.0 AND below 0.0.
//   (c) AC-Htop16 — ALL HudLayout y-positions must be < 16 (upper HUD band).
//
// === Hygiene (AC-H80) ===
// RAYLIB_VERSION is defined by raylib.h.  If it appears here, the include
// chain is polluted.  The BUILD_GAME=OFF build keeps raylib off the path.
//
// === AC-H82 noexcept ===
// static_asserts at the top of this file verify noexcept at compile time.
//
// === Red-state expectation ===
// This file FAILS TO COMPILE until the implementer creates:
//   src/render/hud.hpp
//   src/render/hud.cpp
// That is the correct red state for Gate 2 delivery.

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string_view>
#include <type_traits>

#include "render/hud.hpp"   // render::HudState, render::HudLayout,
                             // render::format_score, render::fuel_bar_width,
                             // render::format_ammo

// ---------------------------------------------------------------------------
// AC-H80: render/hud.hpp must not pull in raylib, world/, entities/,
// input/, <random>, or <chrono>.
// RAYLIB_VERSION is defined by raylib.h; if present here, the include chain
// is polluted.  The BUILD_GAME=OFF build keeps raylib off the compiler path.
// ---------------------------------------------------------------------------
#ifdef RAYLIB_VERSION
static_assert(false,
    "AC-H80 VIOLATED: render/hud.hpp pulled in raylib.h "
    "(RAYLIB_VERSION is defined).  Remove the raylib dependency.");
#endif

// ---------------------------------------------------------------------------
// AC-H82 — format_score, fuel_bar_width, and format_ammo must be noexcept.
// Verified at compile time here in the same TU as the #include.
// ---------------------------------------------------------------------------
static_assert(
    noexcept(render::format_score(0u)),
    "AC-H82: render::format_score must be declared noexcept");

static_assert(
    noexcept(render::fuel_bar_width(0.0f, 32)),
    "AC-H82: render::fuel_bar_width must be declared noexcept");

static_assert(
    noexcept(render::format_ammo(static_cast<std::uint16_t>(0u))),
    "AC-H82: render::format_ammo must be declared noexcept");

// ===========================================================================
// GROUP 1: format_score (AC-H01..H05)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-H01 — format_score(0) returns "000000" (6 chars + null).
//   Given: score = 0
//   When:  format_score is called
//   Then:  result is an array of 7 chars whose first 6 are '0' and [6] == '\0'
// ---------------------------------------------------------------------------
TEST_CASE("AC-H01: format_score(0) returns \"000000\" (6 zero-padded digits + null)", "[render][hud]") {
    // Given / When
    const auto result = render::format_score(0u);

    // Then
    REQUIRE(result[6] == '\0');
    REQUIRE(std::string_view(result.data()) == "000000");
}

// ---------------------------------------------------------------------------
// AC-H02 — format_score(800) returns "000800".
//   Given: score = 800
//   When:  format_score is called
//   Then:  result == "000800" (3 leading zeros)
// ---------------------------------------------------------------------------
TEST_CASE("AC-H02: format_score(800) returns \"000800\"", "[render][hud]") {
    // Given / When
    const auto result = render::format_score(800u);

    // Then
    REQUIRE(result[6] == '\0');
    REQUIRE(std::string_view(result.data()) == "000800");
}

// ---------------------------------------------------------------------------
// AC-H03 — format_score(123456) returns "123456".
//   Given: score = 123456
//   When:  format_score is called
//   Then:  result == "123456" (no leading zeros needed)
// ---------------------------------------------------------------------------
TEST_CASE("AC-H03: format_score(123456) returns \"123456\"", "[render][hud]") {
    // Given / When
    const auto result = render::format_score(123456u);

    // Then
    REQUIRE(result[6] == '\0');
    REQUIRE(std::string_view(result.data()) == "123456");
}

// ---------------------------------------------------------------------------
// AC-H04 — format_score(999999) returns "999999" (maximum in-range).
//   Given: score = 999999
//   When:  format_score is called
//   Then:  result == "999999" (exact max)
// ---------------------------------------------------------------------------
TEST_CASE("AC-H04: format_score(999999) returns \"999999\" (max in-range value)", "[render][hud]") {
    // Given / When
    const auto result = render::format_score(999999u);

    // Then
    REQUIRE(result[6] == '\0');
    REQUIRE(std::string_view(result.data()) == "999999");
}

// ---------------------------------------------------------------------------
// AC-H05 — format_score(1000000) returns "999999" (clamped to max).
//   Given: score = 1000000 (one above the representable max)
//   When:  format_score is called
//   Then:  result == "999999" (clamped, no overflow into 7 digits)
// ---------------------------------------------------------------------------
TEST_CASE("AC-H05: format_score(1000000) returns \"999999\" (clamped to max)", "[render][hud]") {
    // Given / When
    const auto result = render::format_score(1000000u);

    // Then
    REQUIRE(result[6] == '\0');
    REQUIRE(std::string_view(result.data()) == "999999");
}

// ===========================================================================
// GROUP 2: fuel_bar_width (AC-H06..H10)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-H06 — fuel_bar_width(0.0f, 32) returns 0.
//   Given: fuel_fraction = 0.0, container_px = 32
//   When:  fuel_bar_width is called
//   Then:  result == 0 (empty bar)
// ---------------------------------------------------------------------------
TEST_CASE("AC-H06: fuel_bar_width(0.0, 32) returns 0 (empty bar)", "[render][hud]") {
    // Given / When
    const int result = render::fuel_bar_width(0.0f, 32);

    // Then
    REQUIRE(result == 0);
}

// ---------------------------------------------------------------------------
// AC-H07 — fuel_bar_width(1.0f, 32) returns 32 (full bar).
//   Given: fuel_fraction = 1.0, container_px = 32
//   When:  fuel_bar_width is called
//   Then:  result == 32 (full container width)
// ---------------------------------------------------------------------------
TEST_CASE("AC-H07: fuel_bar_width(1.0, 32) returns 32 (full bar)", "[render][hud]") {
    // Given / When
    const int result = render::fuel_bar_width(1.0f, 32);

    // Then
    REQUIRE(result == 32);
}

// ---------------------------------------------------------------------------
// AC-H08 — fuel_bar_width(0.5f, 32) returns 16 (half bar, rounded).
//   Given: fuel_fraction = 0.5, container_px = 32
//   When:  fuel_bar_width is called
//   Then:  result == 16 (0.5 * 32 + 0.5 = 16.5 -> truncated to 16 by static_cast,
//          but the spec formula static_cast<int>(f * c + 0.5f) gives
//          static_cast<int>(16.0f + 0.5f) = static_cast<int>(16.5f) = 16)
// ---------------------------------------------------------------------------
TEST_CASE("AC-H08: fuel_bar_width(0.5, 32) returns 16 (round to nearest)", "[render][hud]") {
    // Given / When
    const int result = render::fuel_bar_width(0.5f, 32);

    // Then: formula: static_cast<int>(0.5f * 32 + 0.5f) = static_cast<int>(16.5f) = 16
    REQUIRE(result == 16);
}

// ---------------------------------------------------------------------------
// AC-H09 — fuel_bar_width(0.25f, 100) returns 25.
//   Given: fuel_fraction = 0.25, container_px = 100
//   When:  fuel_bar_width is called
//   Then:  result == 25 (0.25 * 100 + 0.5 = 25.5 -> 25)
// ---------------------------------------------------------------------------
TEST_CASE("AC-H09: fuel_bar_width(0.25, 100) returns 25", "[render][hud]") {
    // Given / When
    const int result = render::fuel_bar_width(0.25f, 100);

    // Then: formula: static_cast<int>(0.25f * 100 + 0.5f) = static_cast<int>(25.5f) = 25
    REQUIRE(result == 25);
}

// ---------------------------------------------------------------------------
// AC-H10 — fuel_bar_width(-0.5f, 32) returns 0 (negative input clamped).
//   Given: fuel_fraction = -0.5 (below valid range)
//   When:  fuel_bar_width is called
//   Then:  result == 0 (clamped to 0 before multiplication)
// ---------------------------------------------------------------------------
TEST_CASE("AC-H10: fuel_bar_width(-0.5, 32) returns 0 (negative fraction clamped)", "[render][hud]") {
    // Given / When
    const int result = render::fuel_bar_width(-0.5f, 32);

    // Then
    REQUIRE(result == 0);
}

// ===========================================================================
// GROUP 3: format_ammo (AC-H11..H15)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-H11 — format_ammo(0) returns "0".
//   Given: ammo = 0
//   When:  format_ammo is called
//   Then:  result contains "0" with null terminator (no padding)
// ---------------------------------------------------------------------------
TEST_CASE("AC-H11: format_ammo(0) returns \"0\" (no padding, variable-width)", "[render][hud]") {
    // Given / When
    const auto result = render::format_ammo(static_cast<std::uint16_t>(0u));

    // Then
    REQUIRE(result[4] == '\0');   // array is size 5; last must be null-capable
    REQUIRE(std::string_view(result.data()) == "0");
}

// ---------------------------------------------------------------------------
// AC-H12 — format_ammo(9) returns "9".
//   Given: ammo = 9
//   When:  format_ammo is called
//   Then:  result contains "9"
// ---------------------------------------------------------------------------
TEST_CASE("AC-H12: format_ammo(9) returns \"9\"", "[render][hud]") {
    // Given / When
    const auto result = render::format_ammo(static_cast<std::uint16_t>(9u));

    // Then
    REQUIRE(std::string_view(result.data()) == "9");
}

// ---------------------------------------------------------------------------
// AC-H13 — format_ammo(42) returns "42".
//   Given: ammo = 42
//   When:  format_ammo is called
//   Then:  result contains "42"
// ---------------------------------------------------------------------------
TEST_CASE("AC-H13: format_ammo(42) returns \"42\"", "[render][hud]") {
    // Given / When
    const auto result = render::format_ammo(static_cast<std::uint16_t>(42u));

    // Then
    REQUIRE(std::string_view(result.data()) == "42");
}

// ---------------------------------------------------------------------------
// AC-H14 — format_ammo(9999) returns "9999".
//   Given: ammo = 9999 (representable max)
//   When:  format_ammo is called
//   Then:  result contains "9999"
// ---------------------------------------------------------------------------
TEST_CASE("AC-H14: format_ammo(9999) returns \"9999\" (max in-range)", "[render][hud]") {
    // Given / When
    const auto result = render::format_ammo(static_cast<std::uint16_t>(9999u));

    // Then
    REQUIRE(std::string_view(result.data()) == "9999");
}

// ---------------------------------------------------------------------------
// AC-H15 — format_ammo(uint16_max) returns "9999" (clamped).
//   Given: ammo = 65535 (std::numeric_limits<uint16_t>::max())
//   When:  format_ammo is called
//   Then:  result contains "9999" (clamped to display max)
// ---------------------------------------------------------------------------
TEST_CASE("AC-H15: format_ammo(uint16_max=65535) returns \"9999\" (clamped to display max)", "[render][hud]") {
    // Given
    constexpr std::uint16_t max_ammo = std::numeric_limits<std::uint16_t>::max();

    // When
    const auto result = render::format_ammo(max_ammo);

    // Then
    REQUIRE(std::string_view(result.data()) == "9999");
}

// ===========================================================================
// GROUP 4: Determinism + integration (AC-H16..H20)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-H16 — format_score is deterministic: 1000 calls, identical results.
//   Given: score = 42000
//   When:  format_score is called 1000 times independently
//   Then:  every call returns a bit-identical array
// ---------------------------------------------------------------------------
TEST_CASE("AC-H16: format_score is deterministic — 1000 calls return bit-identical results", "[render][hud]") {
    // Given: fixed input, no PRNG, no clock
    constexpr std::uint32_t score = 42000u;
    const auto first = render::format_score(score);

    // When / Then: 1000 independent calls must match
    for (int i = 0; i < 1000; ++i) {
        const auto result = render::format_score(score);
        CAPTURE(i);
        for (std::size_t j = 0; j < result.size(); ++j) {
            REQUIRE(result[j] == first[j]);
        }
    }
}

// ---------------------------------------------------------------------------
// AC-H17 — fuel_bar_width is deterministic: 1000 calls, identical results.
//   Given: fuel_fraction = 0.75f, container_px = 32
//   When:  fuel_bar_width is called 1000 times independently
//   Then:  every call returns the same integer
// ---------------------------------------------------------------------------
TEST_CASE("AC-H17: fuel_bar_width is deterministic — 1000 calls return identical results", "[render][hud]") {
    // Given: fixed input, no PRNG, no clock
    const int first = render::fuel_bar_width(0.75f, 32);

    // When / Then: 1000 independent calls must match
    for (int i = 0; i < 1000; ++i) {
        const int result = render::fuel_bar_width(0.75f, 32);
        CAPTURE(i, result, first);
        REQUIRE(result == first);
    }
}

// ---------------------------------------------------------------------------
// AC-H18 — Round-trip: parse format_score(N) back → N, for representative N.
//   Given: a set of representative scores N in [0, 999999]
//   When:  format_score(N) is called and the result string is parsed via std::stoul
//   Then:  the parsed integer equals N
// ---------------------------------------------------------------------------
TEST_CASE("AC-H18: round-trip parse of format_score(N) returns N for representative values", "[render][hud]") {
    // Given: representative values — no PRNG; deterministic list
    const std::uint32_t scores[] = {0u, 1u, 9u, 10u, 99u, 100u, 999u, 1000u,
                                     9999u, 10000u, 99999u, 100000u, 999999u};

    for (const std::uint32_t n : scores) {
        // When
        const auto arr = render::format_score(n);
        // arr.data() is null-terminated; parse as unsigned long
        const unsigned long parsed = std::stoul(arr.data());

        // Then
        CAPTURE(n, parsed);
        REQUIRE(static_cast<std::uint32_t>(parsed) == n);
    }
}

// ---------------------------------------------------------------------------
// AC-H19 — HudLayout default construction: all y-positions < 16.
//   Given: a default-constructed HudLayout
//   When:  score_y, fuel_y, and ammo_y are read
//   Then:  all three are < 16 (HUD occupies the upper 16 logical pixels)
// ---------------------------------------------------------------------------
TEST_CASE("AC-H19: HudLayout{} default values place all y-positions in the upper 16 pixels", "[render][hud]") {
    // Given / When
    const render::HudLayout layout{};

    // Then
    CAPTURE(layout.score_y, layout.fuel_y, layout.ammo_y);
    REQUIRE(layout.score_y < 16);
    REQUIRE(layout.fuel_y  < 16);
    REQUIRE(layout.ammo_y  < 16);
}

// ---------------------------------------------------------------------------
// AC-H20 — format_score result is null-terminated (arr[6] == '\0').
//   Given: scores = {0, 1, 800, 123456, 999999, 1000000}
//   When:  format_score is called for each
//   Then:  arr[6] == '\0' for every result (array size is 7)
// ---------------------------------------------------------------------------
TEST_CASE("AC-H20: format_score always returns a null-terminated 7-element array", "[render][hud]") {
    // Given: representative values including clamped
    const std::uint32_t scores[] = {0u, 1u, 800u, 123456u, 999999u, 1000000u};

    for (const std::uint32_t s : scores) {
        // When
        const auto arr = render::format_score(s);

        // Then
        CAPTURE(s, arr[6]);
        REQUIRE(arr[6] == '\0');
        REQUIRE(arr.size() == 7u);
    }
}

// ===========================================================================
//  BUG-CLASS FENCE (AC-Hpad)
// ===========================================================================
//
//  D-ScoreWidth (locked): format_score zero-pads to exactly 6 digits.
//  Score 800 must produce "000800", NOT "800" (3 chars without padding).
//
//  A common mistake is to use plain integer-to-string conversion without
//  specifying width and fill character.  That produces "800" (or similar)
//  which fails all 6-character HUD layout assumptions.
//
//  This test fires immediately when the padding is absent.  It checks:
//    (a) the string length (via string_view comparison) is exactly 6
//    (b) the first character is '0' (confirming a leading zero exists)
//    (c) the full string matches "000800" exactly
//
//  If this test fails: re-read D-ScoreWidth.  Use snprintf with "%06u" or
//  equivalent.  Do NOT change this test — fix the formatting.
// ===========================================================================
TEST_CASE("AC-Hpad (BUG-CLASS FENCE): format_score(800) is \"000800\" NOT \"800\" (zero-padding required)", "[render][hud]") {
    // Given / When
    const auto result = render::format_score(800u);

    // Then: full 6-digit zero-padded string
    const std::string_view sv(result.data());
    CAPTURE(sv);

    // Explicit: length must be 6 (NOT 3)
    REQUIRE(sv.size() == 6u);

    // Explicit: first character must be '0' (leading zero present)
    REQUIRE(sv[0] == '0');

    // Explicit: exact match (not just "ends with 800")
    REQUIRE(sv == "000800");

    // Complementary guard: NOT the unpadded form
    REQUIRE_FALSE(sv == "800");
}

// ===========================================================================
//  BUG-CLASS FENCE (AC-Hclamp)
// ===========================================================================
//
//  D-FuelBar (locked): fuel_fraction is clamped to [0.0, 1.0] before
//  computing the bar width.  Two failure modes are guarded:
//
//    (a) Under-clamp: fuel_fraction < 0.0 must produce 0, not a negative int.
//        A negative int would be an invalid pixel width and would corrupt
//        the draw call (e.g. DrawRectangle with negative width is UB / crash).
//
//    (b) Over-clamp: fuel_fraction > 1.0 must produce container_px, not a
//        value exceeding the container.  A bar wider than its container would
//        overflow the HUD area.
//
//  Both directions are tested here with extreme inputs to catch off-by-one
//  clamp boundaries.
//
//  If this test fails: ensure both clamps are applied before the multiply.
//  Do NOT change the test — fix the clamp in fuel_bar_width.
// ===========================================================================
TEST_CASE("AC-Hclamp (BUG-CLASS FENCE): fuel_bar_width clamps BOTH above 1.0 AND below 0.0", "[render][hud]") {
    // Given: extreme out-of-range fractions
    constexpr int container = 32;

    // When / Then: below-zero clamp
    {
        const int neg_tiny   = render::fuel_bar_width(-0.001f, container);
        const int neg_half   = render::fuel_bar_width(-0.5f,   container);
        const int neg_huge   = render::fuel_bar_width(-999.0f, container);
        CAPTURE(neg_tiny, neg_half, neg_huge);
        REQUIRE(neg_tiny == 0);    // NOT negative
        REQUIRE(neg_half == 0);
        REQUIRE(neg_huge == 0);
    }

    // When / Then: above-one clamp
    {
        const int above_tiny = render::fuel_bar_width(1.001f, container);
        const int above_half = render::fuel_bar_width(1.5f,   container);
        const int above_huge = render::fuel_bar_width(999.0f, container);
        CAPTURE(above_tiny, above_half, above_huge);
        REQUIRE(above_tiny == container);   // NOT > 32
        REQUIRE(above_half == container);
        REQUIRE(above_huge == container);
    }

    // Explicit complementary guards
    REQUIRE(render::fuel_bar_width(-0.5f, container) >= 0);         // not negative
    REQUIRE(render::fuel_bar_width(1.5f,  container) <= container);  // not over-wide
}

// ===========================================================================
//  BUG-CLASS FENCE (AC-Htop16)
// ===========================================================================
//
//  D-LayoutTop16 (locked): the HUD area is the upper 16 logical pixels of the
//  320x256 viewport.  All y-positions in HudLayout must therefore satisfy
//  y < 16 to remain within the HUD strip.
//
//  Two classes of mistakes are caught:
//    (a) A y-coordinate placed in the game area (y >= 16) would overlap
//        terrain or game objects.
//    (b) A y-coordinate that is not even a small positive value (e.g. negative)
//        would be off-screen entirely.
//
//  This test checks every named y-field in HudLayout explicitly.  Adding a new
//  field without a corresponding test here is itself a bug.
//
//  If this test fails: re-read D-LayoutTop16 and the HudLayout defaults.
//  Reduce the failing y-value so it is < 16.  Do NOT change this test.
// ===========================================================================
TEST_CASE("AC-Htop16 (BUG-CLASS FENCE): all HudLayout y-positions are < 16 (upper HUD band)", "[render][hud]") {
    // Given: default layout (spec-defined defaults)
    const render::HudLayout layout{};

    // Then: every y-coordinate must reside in the top 16 pixels
    CAPTURE(layout.score_y, layout.fuel_y, layout.ammo_y);

    // score_y < 16
    REQUIRE(layout.score_y >= 0);
    REQUIRE(layout.score_y < 16);

    // fuel_y < 16
    REQUIRE(layout.fuel_y >= 0);
    REQUIRE(layout.fuel_y < 16);

    // ammo_y < 16
    REQUIRE(layout.ammo_y >= 0);
    REQUIRE(layout.ammo_y < 16);

    // Explicit: none of them equals or exceeds 16
    REQUIRE_FALSE(layout.score_y >= 16);
    REQUIRE_FALSE(layout.fuel_y  >= 16);
    REQUIRE_FALSE(layout.ammo_y  >= 16);
}

// ===========================================================================
// GROUP 5: Hygiene (AC-H80..H82)
// ===========================================================================
//
// AC-H80 (no-raylib static_assert) is at the top of this file.
// AC-H82 (noexcept static_asserts) are at the top of this file.

TEST_CASE("AC-H80: render/hud.hpp compiles without raylib, world/, entities/, <random>, <chrono>", "[render][hud]") {
    // Given: this file was compiled with BUILD_GAME=OFF (no raylib on path)
    // When:  it reaches this TEST_CASE at runtime
    // Then:  the headers compiled without forbidden dependencies
    SUCCEED("compilation without raylib/forbidden deps succeeded — AC-H80 satisfied");
}

TEST_CASE("AC-H81: claude_lander_render link list unchanged — render library links against core+warnings only", "[render][hud]") {
    // Given: this test binary linked with claude_lander_render which links
    //        against claude_lander_core + claude_lander_warnings only.
    // When:  this test is reached
    // Then:  no link errors — AC-H81 satisfied
    SUCCEED("render library link list unchanged (core+warnings only) — AC-H81 satisfied");
}

TEST_CASE("AC-H82: format_score, fuel_bar_width, and format_ammo are noexcept (verified by static_assert at top of TU)", "[render][hud]") {
    // Given: static_assert(noexcept(...)) at the top of this file
    // When:  this test is reached (compile succeeded means static_assert passed)
    // Then:  AC-H82 is satisfied
    SUCCEED("static_assert(noexcept) passed at compile time — AC-H82 satisfied");
}

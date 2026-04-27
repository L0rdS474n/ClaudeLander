// src/render/hud.hpp -- Pass 12 HUD data layer.
//
// Pure formatting functions for the upper-16-pixel HUD strip.  No raylib,
// no world/, no entities/, no <random>, no <chrono>.  Pass 13 will wire
// these formatters into per-frame DrawText/DrawRectangle calls.
//
// Locked design references (see docs/plans/pass-12-hud.md):
//   D-HudPure       all functions noexcept; output is text/numbers only.
//   D-LayoutTop16   HUD area is the upper 16 logical pixels of 320x256.
//   D-ScoreWidth    score zero-padded to 6 digits.
//   D-FuelBar       fuel bar width clamped to [0, container_px].
//   D-AmmoCount     ammo as integer, no padding, clamped at 9999.
//   D-Stateless     no globals.
//
// Hygiene fences:
//   AC-H80  no raylib/world/entities/<random>/<chrono> includes here.
//   AC-H82  format_score / fuel_bar_width / format_ammo are noexcept.

#pragma once

#include <array>
#include <cstdint>

namespace render {

    // Pass 13 will fill these per frame from the live game state.
    struct HudState {
        std::uint32_t score;          // 0..999999
        float         fuel_fraction;  // 0.0..1.0
        std::uint16_t ammo;           // 0..9999
    };

    // Logical pixel positions within the 320x256 viewport.  All y-values
    // satisfy y < 16 so the HUD stays within the reserved top strip
    // (D-LayoutTop16).
    struct HudLayout {
        int score_x = 8;
        int score_y = 4;

        int fuel_x      = 100;
        int fuel_y      = 4;
        int fuel_width  = 32;
        int fuel_height = 8;

        int ammo_x = 200;
        int ammo_y = 4;
    };

    // Format score as a zero-padded 6-digit ASCII string with a trailing
    // null.  Example: 800 -> {'0','0','0','8','0','0','\0'}.  Inputs above
    // 999999 are clamped to 999999 to fit the 6-digit field (D-ScoreWidth).
    std::array<char, 7> format_score(std::uint32_t score) noexcept;

    // Pixel width of the fuel bar's filled region given a container of
    // container_px pixels.  fuel_fraction is clamped to [0.0, 1.0] before
    // the multiply (D-FuelBar) so the result is always in [0, container_px]
    // and never negative.  Rounds to nearest via the +0.5 trick.
    int fuel_bar_width(float fuel_fraction, int container_px) noexcept;

    // Format ammo as a variable-width 1..4 digit ASCII string with trailing
    // null.  Example: 0 -> "0", 42 -> "42", 9999 -> "9999".  Inputs above
    // 9999 are clamped to 9999 (D-AmmoCount).
    std::array<char, 5> format_ammo(std::uint16_t ammo) noexcept;

}  // namespace render

// src/render/hud.cpp -- Pass 12 HUD formatters.
//
// Pure noexcept implementations of the score / fuel-bar / ammo formatters
// declared in render/hud.hpp.  No raylib, no world/, no entities/, no
// <random>, no <chrono>.  See docs/plans/pass-12-hud.md for the locked
// design decisions referenced below.

#include "render/hud.hpp"

#include <algorithm>
#include <cmath>

namespace render {

    // D-ScoreWidth: zero-pad to exactly 6 digits.  Clamp at 999999 so the
    // 6-digit field cannot overflow.  Writes digits least-significant-first
    // into positions [0..5] and terminates at [6].
    std::array<char, 7> format_score(std::uint32_t score) noexcept {
        std::uint32_t s = std::min<std::uint32_t>(score, 999999u);
        std::array<char, 7> out{};
        for (int i = 5; i >= 0; --i) {
            out[static_cast<std::size_t>(i)] =
                static_cast<char>('0' + s % 10u);
            s /= 10u;
        }
        out[6] = '\0';
        return out;
    }

    // D-FuelBar: clamp the fraction to [0, 1] BEFORE the multiply so the
    // result cannot be negative or exceed container_px.  Rounds to nearest
    // using the classic +0.5 trick (truncation toward zero of a non-negative
    // float = floor; floor(x + 0.5) = round-half-up for non-negatives).
    int fuel_bar_width(float fuel_fraction, int container_px) noexcept {
        float f = fuel_fraction;
        if (f < 0.0f) f = 0.0f;
        if (f > 1.0f) f = 1.0f;
        return static_cast<int>(f * static_cast<float>(container_px) + 0.5f);
    }

    // D-AmmoCount: variable-width 1..4 digit string, no padding, clamped at
    // 9999.  Special-cased zero so the digit-counting loop never produces an
    // empty string.
    std::array<char, 5> format_ammo(std::uint16_t ammo) noexcept {
        std::uint16_t a = std::min<std::uint16_t>(ammo, std::uint16_t{9999});
        std::array<char, 5> out{};

        if (a == 0) {
            out[0] = '0';
            out[1] = '\0';
            return out;
        }

        // Count digits first so we can write left-to-right with the correct
        // length and place the null terminator immediately after the last
        // digit.
        int digits = 0;
        std::uint16_t tmp = a;
        while (tmp > 0) {
            tmp = static_cast<std::uint16_t>(tmp / 10u);
            ++digits;
        }

        for (int i = digits - 1; i >= 0; --i) {
            out[static_cast<std::size_t>(i)] =
                static_cast<char>('0' + a % 10u);
            a = static_cast<std::uint16_t>(a / 10u);
        }
        out[static_cast<std::size_t>(digits)] = '\0';
        return out;
    }

}  // namespace render

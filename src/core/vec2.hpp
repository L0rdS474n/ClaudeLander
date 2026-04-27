// src/core/vec2.hpp -- Pass 3 header-only Vec2 type.
//
// Header-only by D-Vec2 (docs/plans/pass-3-projection.md): no vec2.cpp.
// All operators and helpers are constexpr and defined inline.  The translation
// unit list of claude_lander_core therefore stays unchanged when this header
// is added.
//
// Style mirrors src/core/vec3.hpp.  Minimal surface for Pass 3: arithmetic
// operators + approx_equal.  Additional helpers can be added when later
// modules need them (KOQ-5).
//
// Column convention: Vec2 is screen-space (px), Y-DOWN, top-left origin
// (matches Mode 13 and raylib 2D).  See docs/adr/0006-pass-3-y-flip-no-negation.md.

#pragma once

// ---------------------------------------------------------------------------
// Vec2 struct
// ---------------------------------------------------------------------------

struct Vec2 {
    float x, y;
};

// ---------------------------------------------------------------------------
// Arithmetic operators -- constexpr, defined inline (required by standard)
// ---------------------------------------------------------------------------

constexpr Vec2 operator+(Vec2 a, Vec2 b) noexcept {
    return { a.x + b.x, a.y + b.y };
}

constexpr Vec2 operator-(Vec2 a, Vec2 b) noexcept {
    return { a.x - b.x, a.y - b.y };
}

constexpr Vec2 operator-(Vec2 a) noexcept {
    return { -a.x, -a.y };
}

constexpr Vec2 operator*(Vec2 v, float s) noexcept {
    return { v.x * s, v.y * s };
}

constexpr Vec2 operator*(float s, Vec2 v) noexcept {
    return { s * v.x, s * v.y };
}

// ---------------------------------------------------------------------------
// Approximate equality helper
//
// Uses combined absolute + relative tolerance:
//   bound_i = eps * (1.0f + max(|a_i|, |b_i|))
// i.e. an absolute slack of `eps` plus a relative slack of `eps * |operand|`.
// This absorbs IEEE-754 rounding of `eps` itself: callers passing eps==1e-5f
// also accept inputs that differ by one representable float step at scale ~1
// (e.g. (1.0f, 1.0f + 1e-5f) where the actual decoded diff is ~1.001e-5f
// because `1e-5f` is not representable exactly).  AC-R04 boundary case
// verifies this; the same case rejects diffs of order 1e-3 at scale 1.
// ---------------------------------------------------------------------------

inline constexpr bool approx_equal(Vec2 a, Vec2 b, float eps = 1e-5f) noexcept {
    auto abs_f = [](float f) constexpr -> float { return f < 0.0f ? -f : f; };
    auto max_f = [](float p, float q) constexpr -> float { return p > q ? p : q; };
    const float bound_x = eps * (1.0f + max_f(abs_f(a.x), abs_f(b.x)));
    const float bound_y = eps * (1.0f + max_f(abs_f(a.y), abs_f(b.y)));
    return abs_f(a.x - b.x) <= bound_x &&
           abs_f(a.y - b.y) <= bound_y;
}

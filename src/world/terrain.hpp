// src/world/terrain.hpp -- Pass 2 procedural terrain.
//
// Deterministic, pure-function altitude field plus a 13x11 mesh sampler.
// The altitude formula is a six-term Fourier sum of integer-argument sines
// driven by the Q31 sin LUT in src/core/lookup_tables.hpp; see ADR-0004 for
// the layer-boundary contract and ADR-0005 for the open question (KOQ-1)
// about the formula's vertical range vs. the bbcelite prose.
//
// Layer rules:
//   world/  --> core/   (one-way; no raylib, no platform headers)
//   No <random>, no <chrono>, no PRNG: altitude is a pure function of (x, z).
//
// Y-DOWN convention: this header returns "altitude" as a height value; the
// per-axis Y-DOWN flip lives in core/, never here.

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "core/vec3.hpp"

namespace terrain {

// ---------------------------------------------------------------------------
// Locked design constants (planner D1)
// ---------------------------------------------------------------------------
//
//   LAND_MID_HEIGHT = 5.0  (tiles)        — vertical mid-point
//   TILE_SIZE       = 1.0  (world units)  — one tile per vertex step
//   SEA_LEVEL       = 5.3125              — = 0x05500000 / 0x01000000
//
// The hex constants come from the original ARM2 16.16 fixed-point format
// (see docs/research/pass-2-terrain-spec.md).  This module ships the float
// scale only; conversion lives at this single boundary per the
// single-flip-point rule in docs/ARCHITECTURE.md.

inline constexpr float LAND_MID_HEIGHT = 5.0f;
inline constexpr float TILE_SIZE       = 1.0f;
inline constexpr float SEA_LEVEL       = 5.3125f;

// ---------------------------------------------------------------------------
// Mesh dimensions (planner D3, D4)
// ---------------------------------------------------------------------------
//
//   13 columns x 11 rows = 143 vertices
//   12 x 10 quads = 120 quads (each quad = 2 triangles when rendered)
//
// Centre vertex is at index (5*13 + 6) = 71.  The mesh snaps to the integer
// tile lattice beneath the ship; build_mesh anchors at
// (floor(centre_x), floor(centre_z)).  Row-major:
//   index = row * MESH_COLS + col
// where row in [0, MESH_ROWS) maps to z-offset [-5, +5] and
//       col in [0, MESH_COLS) maps to x-offset [-6, +6].
//
// std::size_t to match the unsigned comparisons in tests/test_terrain.cpp
// (per architecture review binding requirement).

inline constexpr std::size_t MESH_COLS         = 13;
inline constexpr std::size_t MESH_ROWS         = 11;
inline constexpr std::size_t MESH_VERTEX_COUNT = MESH_COLS * MESH_ROWS;  // = 143
inline constexpr std::size_t MESH_QUAD_COUNT   = (MESH_COLS - 1) * (MESH_ROWS - 1);  // = 120

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
//
// altitude(x, z): height of the terrain at world position (x, z).
//   Pure function; deterministic in (x, z) only.
//   Float inputs are floored to integer tile-corners via
//     static_cast<int32_t>(std::floor(x))
//   so altitude(0.5f, 0.0f) == altitude(0.0f, 0.0f) (planner KOQ-3).
//   Periodic with period 1024 in both axes.
//   Output range: LAND_MID_HEIGHT +/- 10/256 ~= 5.0 +/- 0.039 (planner D4;
//   open contradiction with bbcelite prose recorded in ADR-0005).

float altitude(float x, float z) noexcept;

// build_mesh(centre_x, centre_z): produce a 13x11 grid of Vec3 vertices
// snapped to the integer-tile lattice beneath (centre_x, centre_z).  Each
// vertex's y is altitude(vertex.x, vertex.z).  Row-major ordering.

std::array<Vec3, MESH_VERTEX_COUNT>
build_mesh(float centre_x, float centre_z) noexcept;

// ---------------------------------------------------------------------------
// detail::raw_sum_at -- internal test hook
// ---------------------------------------------------------------------------
//
// Returns the un-divided, un-offset six-term sine sum used inside altitude().
// Exposed so the test suite can verify term decomposition and superposition
// (AC-W13..W18) without re-deriving the sum in every assertion.

namespace detail {

// internal -- test hook only; do not call from non-test code
float raw_sum_at(float x, float z) noexcept;

}  // namespace detail

}  // namespace terrain

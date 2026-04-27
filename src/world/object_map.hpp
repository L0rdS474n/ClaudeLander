// src/world/object_map.hpp -- Pass 11 deterministic ground-object placement.
//
// Defines the public placement query (`object_at`) and the bullet-vs-object
// hit test (`hit_by_bullet`) over a deterministic 256x256 tile grid.  All
// functions are pure; no PRNG state, no clock, no globals -- placement is a
// hash of (tx, tz) only (planner D-PlacementHash, D-Stateless, D-NoLut).
//
// Layer rules (planner D-Boundary, hygiene AC-O80):
//   world/ --> core/ + entities/ + world/terrain.hpp
//   No raylib, no render/, no platform headers, no <random>, no <chrono>.
//
// Compile-time tripwire AC-O80: tests/test_object_map.cpp fires a
// static_assert(false) if RAYLIB_VERSION is defined when this header is
// included -- the only path to defining that macro is including raylib.h
// transitively.  Keep this header free of such inclusions.
//
// Y-DOWN convention: position.y is "altitude" from terrain::altitude, which
// returns a height value (positive up, see ADR-0004).  Object placement does
// not flip the sign; the per-axis Y-DOWN flip lives in core/, never here.

#pragma once

#include <cstdint>
#include <optional>

#include "core/vec3.hpp"
#include "entities/ground_object.hpp"

namespace world {

// ---------------------------------------------------------------------------
// Locked placement constants (planner D-MapSize, D-LaunchpadCarveout)
// ---------------------------------------------------------------------------
//
//   kObjectMapSize = 256        -- side length of the deterministic tile grid;
//                                  object_at wraps tx, tz mod 256 so the map
//                                  repeats every 256 tiles in both axes.
//
// Tile (0, 0) is the launchpad carve-out: object_at(0, 0) ALWAYS returns
// nullopt regardless of the placement hash (AC-Olaunchpad bug-class fence).
// The ship spawns at world position {0, alt(0,0), 0}; an object at the
// launchpad would force an unavoidable game-start collision.

inline constexpr int kObjectMapSize = 256;

// ---------------------------------------------------------------------------
// object_at -- deterministic per-tile placement query (planner §3)
// ---------------------------------------------------------------------------
//
// Returns the GroundObject placed at tile (tx, tz), or nullopt if the tile is
// empty.  Pure function of (tx, tz) only -- no PRNG state, no clock, no
// globals.  noexcept (AC-O82).
//
// Inputs are normalised to [0, kObjectMapSize) via positive-mod (negatives
// wrap; AC-O08 verifies object_at(0,0) == object_at(256,0)).  The launchpad
// carve-out is applied AFTER wrapping so any tile that wraps to (0,0) is also
// empty (AC-Olaunchpad fence + AC-O08 wrap-to-launchpad invariant).
//
// Placement formula (LOCKED, planner D-PlacementHash, D-Density):
//   hash = (uint32_t(tx) * 73856093u) ^ (uint32_t(tz) * 19349663u)
//   if (hash % 100 < 30)   -> populated (~30% density)
//   type = ObjectType((hash >> 8) % 5 + 1)   -> Tree..FirTree (skip None=0)
//   position = {tx*TILE_SIZE, altitude(x,z), tz*TILE_SIZE}

std::optional<entities::GroundObject> object_at(int tx, int tz) noexcept;

// ---------------------------------------------------------------------------
// hit_by_bullet -- bullet-vs-object collision (planner §3, AC-O15..O18)
// ---------------------------------------------------------------------------
//
// Returns true iff the bullet's world-space position is within (or at) a
// 0.5 * TILE_SIZE radius of any populated tile's object position.  noexcept
// (AC-O82).
//
// Implementation: 3x3 tile-neighbourhood scan around floor(b.xz / TILE_SIZE).
// The boundary at exactly 0.5 tiles is INCLUSIVE (planner §4 AC-O18: "true at
// exact 0.5"); use squared distance with `<=` to avoid the boundary flip.

bool hit_by_bullet(Vec3 bullet_position) noexcept;

}  // namespace world

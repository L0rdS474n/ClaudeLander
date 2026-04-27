// src/world/camera_follow.hpp -- Pass 7 follow-camera position function.
//
// Pure, stateless, deterministic camera position derived from the ship's
// world position alone.  No raylib, no render/, no entities/, no input/,
// no <random>, no <chrono>.  Layer rule: world/ --> core/ + world/terrain.
//
// Camera formula (locked: D-BackOffsetSign, D-GroundClampViaTerrain):
//   cam.x   = ship.x
//   cam.z   = ship.z - kCameraBackOffset * terrain::TILE_SIZE
//   floor_y = terrain::altitude(cam.x, cam.z) + kCameraGroundClearance
//   cam.y   = std::max(ship.y, floor_y)
//
// The function is noexcept and has no orientation output (D-NoCameraTilt,
// D-Stateless).  See docs/plans/pass-7-camera-follow.md for the AC list and
// design constants.

#pragma once

#include "core/vec3.hpp"

namespace world {

// ---------------------------------------------------------------------------
// Locked design constants (planner D-BackOffsetSign, D-GroundClampViaTerrain)
// ---------------------------------------------------------------------------
//
//   kCameraBackOffset      -- camera sits this many TILE_SIZE units BEHIND
//                             the ship along +z (i.e. cam.z < ship.z).
//   kCameraGroundClearance -- minimum camera height above the terrain floor
//                             at the camera's (x, z) position.

inline constexpr float kCameraBackOffset      = 5.0f;
inline constexpr float kCameraGroundClearance = 0.1f;

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
//
// follow_camera_position(ship_position): compute the camera world position
// that follows the ship.  Pure function of ship_position only; no globals,
// no clocks, no PRNG.  Returns Vec3 (no rotation, no orientation struct).

Vec3 follow_camera_position(Vec3 ship_position) noexcept;

}  // namespace world

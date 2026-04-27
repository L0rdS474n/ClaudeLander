// src/physics/kinematics.hpp -- Pass 4 stateless kinematics primitives.
//
// Pure functions on Vec3/Mat3.  No global state, no clocks, no PRNG, no I/O.
// Determinism is the observable property tested by AC-K14 (1000 iterations
// bit-identical across two independent runs); see also AC-K82.
//
// Layer rules:
//   physics/ --> core/  (one-way; no raylib, no world/, no entities/, no
//                        render/, no <random>, no <chrono>)
//   step()  lives in entities/ship, NOT here, because it mutates a Ship.
//   This module only exposes per-frame primitives that compose into step().
//
// Y-DOWN convention: positive y = downward.  apply_gravity ADDS to vy.  Any
// "subtract gravity" rewrite would invert ship motion; AC-K08 is the loud
// guard test for that bug class.
//
// Per-frame contract: kDragPerFrame, kGravityPerFrame, kFullThrust, and
// kHalfThrust are all "per frame" magnitudes (planner D-PerFrame).  No dt
// multiplication happens inside this module.  kFrameDt is exposed only so
// callers that need to convert to per-second units can do so at the boundary.
//
// Tripwire: physics_no_forbidden_includes (CMakeLists.txt) greps this
// directory for raylib/world/entities/render/<random>/<chrono>.

#pragma once

#include "core/matrix3.hpp"
#include "core/vec3.hpp"

namespace physics {

// ---------------------------------------------------------------------------
// Locked numerical constants (planner D-Constants)
// ---------------------------------------------------------------------------
//
// kDragPerFrame    = 63/64    -- float-equivalent of ARM `v - (v >> 6)`.
// kFullThrust      = 1/2048   -- ARM `>> 11` divisor on the full-thrust path.
// kHalfThrust      = 1/8192   -- = Full / 4 (NOT Full / 2; AC-K10 fence).
// kGravityPerFrame = 1/4096   -- planner default; one order smaller than full
//                                thrust so thrust easily overcomes gravity.
// kFrameRate       = 50 Hz    -- PAL Mode-13 cadence on the original.
// kFrameDt         = 1/50 s   -- exposed for callers needing per-second.

inline constexpr float kDragPerFrame    = 63.0f / 64.0f;
inline constexpr float kFullThrust      = 1.0f / 2048.0f;
inline constexpr float kHalfThrust      = 1.0f / 8192.0f;
inline constexpr float kGravityPerFrame = 1.0f / 4096.0f;
inline constexpr int   kFrameRate       = 50;
inline constexpr float kFrameDt         = 1.0f / 50.0f;

// ---------------------------------------------------------------------------
// ThrustLevel (planner D-Thrust)
// ---------------------------------------------------------------------------
//
// Three button states.  None = no thrust applied; Half = quarter of full
// (matches the original's left-mouse / middle-mouse / no-press tri-state).

enum class ThrustLevel { None, Half, Full };

// ---------------------------------------------------------------------------
// Public API -- pure stateless per-frame primitives
// ---------------------------------------------------------------------------
//
// apply_drag(v):
//   Returns v scaled componentwise by kDragPerFrame.  All three axes are
//   scaled uniformly (planner D-DragSemantics).
//
// apply_gravity(v):
//   Returns v with kGravityPerFrame ADDED to vy (Y-DOWN; planner
//   D-GravitySemantics).  vx and vz pass through unchanged.
//
// apply_thrust(v, orientation, level):
//   Returns v + orientation.col[1] * factor(level), where col[1] is the
//   ship's roof (body-frame "up", opposite the exhaust plume).  factor maps
//   None -> 0, Half -> kHalfThrust, Full -> kFullThrust.  Catches "world up
//   hardcoded" bugs because thrust must follow the rotated roof, not {0,1,0}
//   (AC-K12 fence).

Vec3 apply_drag   (Vec3 velocity) noexcept;
Vec3 apply_gravity(Vec3 velocity) noexcept;
Vec3 apply_thrust (Vec3 velocity, const Mat3& orientation, ThrustLevel level) noexcept;

}  // namespace physics

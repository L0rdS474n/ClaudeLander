// src/input/mouse_polar.hpp -- Pass 5 stateless mouse-polar input primitives.
//
// Pure functions on Vec2/Mat3.  No global state, no clocks, no PRNG, no I/O.
// Determinism is the observable property tested by AC-I20 (1000 iterations
// bit-identical across two independent runs); see also AC-I82.
//
// Layer rules:
//   input/ --> core/  (one-way; no raylib, no world/, no entities/, no
//                       render/, no physics/, no <random>, no <chrono>)
//   The mouse-to-orientation pipeline is fully stateless: callers thread the
//   ShipAngles state through update_angles() and call build_orientation() to
//   get a Mat3 for downstream physics/render.
//
// Pipeline (planner D-Pipeline):
//   mouse_offset --to_polar--> {radius, angle}
//                                   |
//                              damp + prev
//                                   |
//                            ShipAngles{pitch, yaw}
//                                   |
//                  orientation_from_pitch_yaw / build_orientation
//                                   |
//                                 Mat3
//
// Matrix layout (planner D-MatrixLayout, a=pitch, b=yaw):
//         col[0]=nose          col[1]=roof          col[2]=side
// row 0   cos(a)*cos(b)       -sin(a)*cos(b)        sin(b)
// row 1   sin(a)               cos(a)                0
// row 2  -cos(a)*sin(b)        sin(a)*sin(b)         cos(b)
//
// Damp ratio (planner D-DampRatio): 50/50 blend.  damp(prev, in) = 0.5*prev + 0.5*in.
//
// Tripwire: input_no_forbidden_includes (CMakeLists.txt) greps this directory
// for raylib/world/entities/render/physics/<random>/<chrono>.

#pragma once

#include "core/vec2.hpp"
#include "core/vec3.hpp"
#include "core/matrix3.hpp"

namespace input {

// ---------------------------------------------------------------------------
// PolarOffset -- result of converting a screen-space mouse offset to polar.
// radius >= 0, angle in radians from std::atan2 (range [-pi, pi]).
// ---------------------------------------------------------------------------

struct PolarOffset {
    float radius;
    float angle;
};

// ---------------------------------------------------------------------------
// ShipAngles -- accumulated pitch (a) and yaw (b) state after damping.
// Threaded by the caller frame-to-frame; consumed by build_orientation.
// ---------------------------------------------------------------------------

struct ShipAngles {
    float pitch;
    float yaw;
};

// ---------------------------------------------------------------------------
// Public API -- pure stateless functions.
// ---------------------------------------------------------------------------
//
// to_polar(offset):
//   Returns {sqrt(dx*dx + dy*dy), atan2(dy, dx)}.  Origin input returns
//   exactly {0, 0} (AC-I01).  Large magnitudes remain finite (AC-I04).
//
// damp(prev, input):
//   Returns 0.5f * prev + 0.5f * input.  50/50 blend, no other ratio
//   (AC-Idamp / AC-I06).  NaN propagates from either argument (AC-I09).
//
// orientation_from_pitch_yaw(pitch, yaw):
//   Builds a column-major Mat3 per the matrix layout above.  Identity at
//   zero angles (AC-I10).  det == 1 for any input (AC-I11).
//
// update_angles(prev, mouse_offset):
//   Convenience composition: to_polar(mouse_offset) -> damp each angle
//   against prev -> return new ShipAngles.
//
// build_orientation(angles):
//   Thin wrapper for orientation_from_pitch_yaw(angles.pitch, angles.yaw).
//   Element-wise identical to the direct call (AC-I19).

PolarOffset to_polar(Vec2 offset) noexcept;
float       damp(float prev, float input) noexcept;
Mat3        orientation_from_pitch_yaw(float pitch, float yaw) noexcept;
ShipAngles  update_angles(ShipAngles prev, Vec2 mouse_offset) noexcept;
Mat3        build_orientation(ShipAngles angles) noexcept;

}  // namespace input

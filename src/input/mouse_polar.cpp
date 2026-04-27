// src/input/mouse_polar.cpp -- Pass 5 stateless mouse-polar input primitives.
//
// Implementation notes:
//   - to_polar uses std::atan2(dy, dx).  At (0,0) atan2 returns 0 on glibc /
//     libc++ / MSVC.  AC-I01 locks the contract that origin -> {0, 0}; if a
//     platform deviates we would need to guard the call.  Verified on the
//     project's Linux toolchain.
//   - damp is the canonical 50/50 blend (planner D-DampRatio).  Implemented
//     as `0.5f * prev + 0.5f * input` to match the planner spec verbatim;
//     this also propagates NaN from either argument (AC-I09).
//   - orientation_from_pitch_yaw constructs the column-major matrix per
//     planner D-MatrixLayout (a=pitch, b=yaw).  Each column is filled in the
//     order col[0]=nose, col[1]=roof, col[2]=side.
//
// Boundary: includes <cmath> + the headers from the .hpp only.  No raylib,
// no world/, no entities/, no render/, no physics/, no <random>, no <chrono>.
// Enforced by the input_no_forbidden_includes CTest tripwire.

#include "input/mouse_polar.hpp"

#include "core/matrix3.hpp"
#include "core/vec2.hpp"
#include "core/vec3.hpp"

#include <cmath>

namespace input {

PolarOffset to_polar(Vec2 offset) noexcept {
    const float radius = std::sqrt(offset.x * offset.x + offset.y * offset.y);
    const float angle  = std::atan2(offset.y, offset.x);
    return PolarOffset{ radius, angle };
}

float damp(float prev, float input) noexcept {
    // 50/50 blend.  Any other ratio fails AC-Idamp / AC-I06.
    return 0.5f * prev + 0.5f * input;
}

Mat3 orientation_from_pitch_yaw(float pitch, float yaw) noexcept {
    // a = pitch, b = yaw -- match the locked formula in the .hpp comment.
    const float a = pitch;
    const float b = yaw;
    const float ca = std::cos(a);
    const float sa = std::sin(a);
    const float cb = std::cos(b);
    const float sb = std::sin(b);

    // Column-major Mat3 per planner D-MatrixLayout.
    //
    //         col[0]=nose          col[1]=roof          col[2]=side
    // row 0   cos(a)*cos(b)       -sin(a)*cos(b)        sin(b)
    // row 1   sin(a)               cos(a)                0
    // row 2  -cos(a)*sin(b)        sin(a)*sin(b)         cos(b)

    Mat3 m{};
    // col[0] = nose
    m.col[0] = Vec3{  ca * cb,  sa, -ca * sb };
    // col[1] = roof
    m.col[1] = Vec3{ -sa * cb,  ca,  sa * sb };
    // col[2] = side
    m.col[2] = Vec3{  sb,       0.0f, cb     };
    return m;
}

ShipAngles update_angles(ShipAngles prev, Vec2 mouse_offset) noexcept {
    // 1) Convert the screen offset to polar (radius, angle).
    const PolarOffset p = to_polar(mouse_offset);
    // 2) Damp each accumulated angle 50/50 against the polar result.
    //    pitch <- damp(prev.pitch, radius)
    //    yaw   <- damp(prev.yaw,   angle)
    const float new_pitch = damp(prev.pitch, p.radius);
    const float new_yaw   = damp(prev.yaw,   p.angle);
    return ShipAngles{ new_pitch, new_yaw };
}

Mat3 build_orientation(ShipAngles angles) noexcept {
    // Thin wrapper -- AC-I19 requires element-wise identical output.
    return orientation_from_pitch_yaw(angles.pitch, angles.yaw);
}

}  // namespace input

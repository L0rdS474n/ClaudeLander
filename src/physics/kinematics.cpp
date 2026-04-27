// src/physics/kinematics.cpp -- Pass 4 stateless kinematics primitives.
//
// Y-DOWN reminder: apply_gravity ADDS to vy.  See AC-K08 fence in
// tests/test_kinematics.cpp before "fixing" any sign here.
//
// Boundary: includes core/ headers only.  No raylib, no world/, no entities/,
// no render/, no <random>, no <chrono>.  Enforced by the
// physics_no_forbidden_includes CTest tripwire.

#include "physics/kinematics.hpp"

#include "core/matrix3.hpp"
#include "core/vec3.hpp"

namespace physics {

namespace {

// Resolve a ThrustLevel to its per-frame magnitude.  None deliberately maps
// to exact zero so apply_thrust(v, _, None) is a bit-identical pass-through
// of v (AC-K09).
constexpr float thrust_factor(ThrustLevel level) noexcept {
    switch (level) {
        case ThrustLevel::Full: return kFullThrust;
        case ThrustLevel::Half: return kHalfThrust;
        case ThrustLevel::None: return 0.0f;
    }
    return 0.0f;
}

}  // namespace

Vec3 apply_drag(Vec3 velocity) noexcept {
    return velocity * kDragPerFrame;
}

Vec3 apply_gravity(Vec3 velocity) noexcept {
    // Y-DOWN: positive y is downward, so gravity adds to vy.
    return velocity + Vec3{0.0f, kGravityPerFrame, 0.0f};
}

Vec3 apply_thrust(Vec3 velocity, const Mat3& orientation, ThrustLevel level) noexcept {
    // col[1] is the body-frame roof (opposite the exhaust); thrust acts along
    // it, NOT along world +y.  Custom orientations (AC-K12) verify this.
    return velocity + orientation.col[1] * thrust_factor(level);
}

}  // namespace physics

// src/entities/ship.cpp -- Pass 4 ship state and per-frame integrator.
//
// Step composition order is LOCKED (planner D-StepOrder, AC-K13):
//   drag -> gravity -> thrust -> position += velocity
// Position uses the POST-thrust velocity.  See AC-K13's stale-velocity fence.
//
// Vertex constants are the 9-vertex body-frame mesh from
// docs/research/pass-4-ship-kinematics-spec.md.  Floats are spec-table
// literals (ARM 8.24 fixed-point divided by 0x01000000); AC-S03's kVertexEps
// = 5e-4f tolerance accommodates the rounding in the spec table itself.
//
// Boundary: includes core/ + physics/ headers only.  No raylib, no world/,
// no render/.  Enforced by entities_no_forbidden_includes CTest tripwire.

#include "entities/ship.hpp"

#include <array>

#include "core/vec3.hpp"
#include "physics/kinematics.hpp"

namespace entities {

const std::array<Vec3, kShipVertexCount> kShipVertices = {{
    Vec3{  1.000f,  0.3125f,  0.500f },  // 0
    Vec3{  1.000f,  0.3125f, -0.500f },  // 1
    Vec3{  0.000f,  0.625f,  -0.931f },  // 2
    Vec3{ -0.099f,  0.3125f,  0.000f },  // 3
    Vec3{  0.000f,  0.625f,   1.075f },  // 4
    Vec3{ -0.900f, -0.531f,   0.000f },  // 5
    Vec3{  0.333f,  0.3125f,  0.250f },  // 6
    Vec3{  0.333f,  0.3125f, -0.250f },  // 7
    Vec3{ -0.800f,  0.3125f,  0.000f },  // 8
}};

void step(Ship& ship, physics::ThrustLevel thrust) noexcept {
    ship.velocity = physics::apply_drag(ship.velocity);
    ship.velocity = physics::apply_gravity(ship.velocity);
    ship.velocity = physics::apply_thrust(ship.velocity, ship.orientation, thrust);
    ship.position = ship.position + ship.velocity;
}

}  // namespace entities

// src/entities/rock.cpp -- Pass 10 rock state and per-frame integrator.
//
// Step composition order is LOCKED (planner D-RockPhysics, AC-R05/R06):
//   if (!alive) return;
//   velocity = apply_gravity(velocity);
//   position += velocity;                  -- POST-gravity velocity
//
// Vertex constants are the 6-vertex axis-aligned octahedron (planner default
// from docs/research/pass-10-rock-particles-spec.md §1).  The "6 vertices,
// 8 faces" topology matches the bbcelite Lander rock without copying any
// specific bbcelite vertex table.
//
// Boundary: includes core/ + physics/ headers only.  No raylib, no world/,
// no render/.  Enforced by entities_no_forbidden_includes CTest tripwire.

#include "entities/rock.hpp"

#include <array>

#include "core/vec3.hpp"
#include "physics/kinematics.hpp"

namespace entities {

const std::array<Vec3, kRockVertexCount> kRockVertices = {{
    Vec3{  1.0f,  0.0f,  0.0f },   // +x
    Vec3{ -1.0f,  0.0f,  0.0f },   // -x
    Vec3{  0.0f,  1.0f,  0.0f },   // +y (down in Y-DOWN)
    Vec3{  0.0f, -1.0f,  0.0f },   // -y (up)
    Vec3{  0.0f,  0.0f,  1.0f },   // +z
    Vec3{  0.0f,  0.0f, -1.0f },   // -z
}};

void step(Rock& rock) noexcept {
    if (!rock.alive) return;
    rock.velocity = physics::apply_gravity(rock.velocity);
    rock.position = rock.position + rock.velocity;
}

}  // namespace entities

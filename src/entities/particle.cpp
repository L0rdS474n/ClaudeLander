// src/entities/particle.cpp -- Pass 10 particle state and per-frame integrator.
//
// Step composition order is LOCKED (AC-R11..R20):
//   if (ttl == 0) return;                                      -- AC-R11, AC-R19/AC-Pttl
//   if (kind != Exhaust) velocity = apply_gravity(velocity);   -- AC-R14..R17/AC-Pexh
//   position += velocity;                                      -- POST-gravity velocity
//   --ttl;                                                     -- AC-R12, AC-R13
//
// The early-exit on ttl==0 MUST precede the decrement, otherwise --ttl on a
// uint16_t 0 wraps to 65535 and the particle "revives" silently.  AC-R19 is
// the bug-class fence for that exact mistake.
//
// Boundary: includes core/ + physics/ headers only.  No raylib, no world/,
// no render/.  Enforced by entities_no_forbidden_includes CTest tripwire.

#include "entities/particle.hpp"

#include "core/vec3.hpp"
#include "physics/kinematics.hpp"

namespace entities {

void step(Particle& p) noexcept {
    if (p.ttl == 0) return;
    if (p.kind != ParticleKind::Exhaust) {
        p.velocity = physics::apply_gravity(p.velocity);
    }
    p.position = p.position + p.velocity;
    --p.ttl;
}

}  // namespace entities

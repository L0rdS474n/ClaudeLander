// src/entities/particle.hpp -- Pass 10 particle entity (state + per-frame integrator).
//
// Particle is a plain aggregate.  step() applies the locked composition
//   if (ttl == 0) return;
//   if (kind != Exhaust) velocity = apply_gravity(velocity);
//   position += velocity;
//   --ttl;
// in that order (planner D-ExhaustNoGravity, D-ParticleTTL, AC-R11..R20).
//
// Layer rules:
//   entities/ --> physics/ + core/  (one-way; no raylib, no world/, no render/)
//   step() lives here, not in physics/, because it mutates a Particle.
//
// Y-DOWN convention: positive y = downward.  apply_gravity ADDS to vy.
//
// Bug-class fences:
//   AC-Pttl (AC-R19) -- step on a dead particle (ttl==0) MUST be a complete
//     no-op.  A naive implementation that decrements first would underflow
//     uint16_t (0 -> 65535) and "revive" the particle.
//   AC-Pexh (AC-R20) -- exhaust velocity MUST be preserved across step (only
//     position changes; gravity is NOT applied).  A missing kind guard would
//     leak gravity into smoke trails.
//
// Tripwire: entities_no_forbidden_includes (CMakeLists.txt) greps this
// directory for raylib/world/render.

#pragma once

#include <cstdint>

#include "core/vec3.hpp"

namespace entities {

// ---------------------------------------------------------------------------
// ParticleKind (planner D-ParticleKind)
// ---------------------------------------------------------------------------
//
// Four particle kinds.  Only Exhaust is exempt from gravity (D-ExhaustNoGravity)
// so smoke trails hang in place.  Explosion / Bullet / Spark all receive
// gravity each frame.
//
// Underlying type pinned to uint8_t for compact storage; explicit Exhaust=0
// makes a default-zeroed Particle's kind well-defined.

enum class ParticleKind : std::uint8_t {
    Exhaust   = 0,
    Explosion = 1,
    Bullet    = 2,
    Spark     = 3,
};

// ---------------------------------------------------------------------------
// Particle aggregate (planner D-ParticleTTL)
// ---------------------------------------------------------------------------
//
// Default-initialised state is a "dead" particle:
//   position = {0, 0, 0}
//   velocity = {0, 0, 0}
//   ttl      = 0           -- AC-R11: default ttl=0 -> step is a no-op
//   kind     = Exhaust     -- explicit Exhaust=0 makes this well-defined
//   color    = 0
//
// Aggregate (no virtual, no user-declared constructors) -- AC-R83.

struct Particle {
    Vec3          position { 0.0f, 0.0f, 0.0f };
    Vec3          velocity { 0.0f, 0.0f, 0.0f };
    std::uint16_t ttl      { 0 };
    ParticleKind  kind     { ParticleKind::Exhaust };
    std::uint16_t color    { 0 };
};

// ---------------------------------------------------------------------------
// step -- locked per-frame integrator (AC-R11..R20)
// ---------------------------------------------------------------------------
//
// Composition order (the early-exit MUST come before the decrement to avoid
// uint16_t underflow on dead particles -- AC-R19/AC-Pttl bug-class fence):
//   1. if (ttl == 0) return;                                  -- AC-R11, AC-R19
//   2. if (kind != Exhaust) velocity = apply_gravity(...);    -- AC-R14..R17
//   3. position += velocity;                                  -- POST-gravity vel
//   4. --ttl;                                                 -- AC-R12, R13
//
// Pure function of (Particle); deterministic.  AC-R18 exercises 1000-step
// bit-identical determinism across two independent runs.

void step(Particle& p) noexcept;

}  // namespace entities

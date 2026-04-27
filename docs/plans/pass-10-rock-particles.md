# Pass 10 Rocks + Particles — Planner Output (Gate 1)

**Branch:** `feature/pass-10-rock-particles`
**Date:** 2026-04-27
**Baseline:** 273/273 tests green (Pass 1..9).

## 1. Problem restatement

Two new entity types with per-frame step functions:

- `entities::Rock` — falls under gravity; 6-vertex octahedron mesh.
- `entities::Particle` — short-lived; exhaust/explosion/bullet/spark.

No spawning logic at this layer (Pass 13). No rendering at this layer
(Pass 13). Pure data + step function.

**Module placement:** extends existing `claude_lander_entities`.

## 2. Locked design decisions

| ID | Decision |
|----|----------|
| **D-RockMesh** | 6-vertex axis-aligned octahedron (planner default). |
| **D-RockPhysics** | gravity → position += velocity. No drag, no rotation update at this layer. |
| **D-ParticleKind** | enum with 4 values: Exhaust, Explosion, Bullet, Spark. |
| **D-ParticleTTL** | uint16_t frames; 0 = dead; step decrements. |
| **D-ExhaustNoGravity** | Exhaust particles don't accelerate down. Other kinds do. |
| **D-Stateless** | step() functions are pure modifiers. |
| **D-NoLut** | Use Pass 4 `physics::apply_gravity`. |
| **D-AliveFlag** | `Rock.alive` defaults to true. |

## 3. Public API

```cpp
namespace entities {
    inline constexpr std::size_t kRockVertexCount = 6;
    inline constexpr std::size_t kRockFaceCount   = 8;
    extern const std::array<Vec3, kRockVertexCount> kRockVertices;

    struct Rock {
        Vec3 position{0.0f, -50.0f, 0.0f};
        Vec3 velocity{0.0f, 0.0f, 0.0f};
        Mat3 orientation{ /* identity */ };
        bool alive = true;
    };

    void step(Rock& rock) noexcept;

    enum class ParticleKind : std::uint8_t {
        Exhaust = 0,
        Explosion,
        Bullet,
        Spark,
    };

    struct Particle {
        Vec3 position;
        Vec3 velocity;
        std::uint16_t ttl;
        ParticleKind kind;
        std::uint16_t color;
    };

    void step(Particle& p) noexcept;
}
```

Implementation:
```cpp
void step(Rock& r) noexcept {
    if (!r.alive) return;
    r.velocity = physics::apply_gravity(r.velocity);
    r.position = r.position + r.velocity;
}

void step(Particle& p) noexcept {
    if (p.ttl == 0) return;
    if (p.kind != ParticleKind::Exhaust) {
        p.velocity = physics::apply_gravity(p.velocity);
    }
    p.position = p.position + p.velocity;
    --p.ttl;
}
```

## 4. Acceptance criteria (24)

### Rock (AC-R01..R10)
- AC-R01 — `Rock{}` defaults: position `{0,-50,0}`, velocity zero, identity orient, alive=true.
- AC-R02 — `kRockVertices.size() == 6`.
- AC-R03 — `kRockFaceCount == 8`.
- AC-R04 — Rock vertices are axis-aligned ±1 octahedron (six points: ±x, ±y, ±z).
- AC-R05 — `step(rock)` applies gravity to velocity (matches `apply_gravity`).
- AC-R06 — `step(rock)` integrates position += new_velocity.
- AC-R07 — Determinism: 1000 steps produce bit-identical state across two runs.
- AC-R08 — `alive=false` → step is no-op.
- AC-R09 — After 100 steps from rest: `velocity.y ≈ 100 * kGravityPerFrame`.
- AC-R10 — Bug-class fence (AC-Rgrav): velocity.y is positive after step (Y-DOWN: gravity pulls +y).

### Particle (AC-R11..R20)
- AC-R11 — Default `ttl=0` → step is no-op.
- AC-R12 — `ttl=10` → step decrements ttl to 9.
- AC-R13 — `ttl=1` → step → ttl=0; subsequent step is no-op.
- AC-R14 — Exhaust kind: gravity NOT applied to velocity.
- AC-R15 — Explosion kind: gravity applied to velocity.
- AC-R16 — Bullet kind: gravity applied.
- AC-R17 — Spark kind: gravity applied.
- AC-R18 — Determinism: 1000 steps bit-identical across runs.
- AC-R19 — Bug-class fence (AC-Pttl): step on dead particle (ttl=0) does nothing.
- AC-R20 — Bug-class fence (AC-Pexh): exhaust velocity preserved across step (only position changes).

### Hygiene + boundaries (AC-R80..R83)
- AC-R80 — `src/entities/rock.{hpp,cpp}` and `src/entities/particle.{hpp,cpp}` exclude raylib/world/render/`<random>`/`<chrono>`. Existing `entities_no_forbidden_includes` tripwire covers.
- AC-R81 — Both new files link only to `core` + `physics` + `warnings`.
- AC-R82 — `static_assert(noexcept(step(rock)))` and `static_assert(noexcept(step(particle)))`.
- AC-R83 — `std::is_aggregate_v<Rock>` and `std::is_aggregate_v<Particle>` (POD-like, no virtual).

## 5. Test plan

`tests/test_rock.cpp` — AC-R01..R10. Tag `[entities][rock]`.
`tests/test_particle.cpp` — AC-R11..R20 + R80..R83. Tag `[entities][particle]`.

Tolerances: `kRockEps = 1e-6f`, `kParticleEps = 1e-6f`.

## 6. CMake plan

Append `src/entities/rock.cpp` and `src/entities/particle.cpp` to
`claude_lander_entities`. Append both test files to test sources.

No new tripwires.

## 7. Definition of Done

- [ ] All 24 ACs green.
- [ ] 273-baseline preserved.
- [ ] No forbidden includes in `src/entities/`.
- [ ] noexcept asserts present.
- [ ] PR body: `Closes #TBD-pass-10` + DEFERRED + WIRING.

## 8. ADR triggers

Zero. Same pattern as Pass 4.

## 9. Open questions

- KOQ-1: empirical particle counts/lifetimes → Pass 14.
- KOQ-2: spawn loop / RNG seeding → Pass 13.
- KOQ-3: rendering (mesh + shadow) → Pass 13.
- KOQ-4: rock-vs-ship collision → Pass 11.

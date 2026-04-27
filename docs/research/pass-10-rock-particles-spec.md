# Pass 10 Rocks + Particles — Specification

> Gate 1.5 research output for `feature/pass-10-rock-particles`.
> Prose-only spec from <https://lander.bbcelite.com>.

## Sources read

- <https://lander.bbcelite.com/deep_dives/dropping_rocks_from_the_sky.html>
- <https://lander.bbcelite.com/deep_dives/the_lander_objects.html> (rock = 6 vertices, 8 faces)
- <https://lander.bbcelite.com/deep_dives/the_explosion_effect.html>
- <https://lander.bbcelite.com/deep_dives/the_main_loop.html> §"DropRocksFromTheSky", §"MoveAndDrawParticles"

## 1. Rock model

| field | type | source |
|---|---|---|
| position    | Vec3  | initial spawn at high altitude (planner default y = -50, x/z spread randomly) |
| velocity    | Vec3  | starts at zero; gravity adds per frame |
| orientation | Mat3  | small random rotation; in C++ port, identity is fine for Pass 10 |
| alive       | bool  | turns false on terrain impact |

Mesh: 6 vertices, 8 faces (per bbcelite). Pass 10 stores the vertex
table (planner default: a small octahedron — clean-room equivalent that
preserves "6 vertices, 8 faces"). Face indices may be deferred to Pass
13 if needed.

Octahedron vertices (planner default; spec-equivalent shape):
```
{ 1, 0, 0}  // +x
{-1, 0, 0}  // -x
{ 0, 1, 0}  // +y (down in Y-DOWN)
{ 0,-1, 0}  // -y (up)
{ 0, 0, 1}  // +z
{ 0, 0,-1}  // -z
```

## 2. Rock physics

Per-frame update:
```
rock.velocity += gravity_per_frame    // applies physics::kGravityPerFrame to y
rock.position += rock.velocity        // no drag, no thrust, no rotation
```

Pass 10 reuses `physics::apply_gravity` from Pass 4.

## 3. Rock spawn rule

Bbcelite spawns rocks when **score >= 800**. Spawn rate is random with a
cooldown. Pass 10 ships the rock entity + step function only — the
spawn loop lives in Pass 13 (game loop) and uses the Pass 1 PRNG.

## 4. Particle model

| field | type | meaning |
|---|---|---|
| position  | Vec3  | world-space |
| velocity  | Vec3  | world-space |
| color     | uint16_t | base palette colour |
| ttl       | uint16_t | time-to-live in frames; 0 = dead |
| kind      | enum   | Exhaust / Explosion / Bullet / Spark |

## 5. Particle physics

```
if (ttl == 0) { skip; particle is dead }
particle.velocity += gravity_per_frame  // kind-dependent: exhaust no gravity
particle.position += particle.velocity
particle.ttl -= 1
```

Pass 10 ships the data structures + per-frame step function. Spawning
and rendering land in Pass 13.

## 6. Public API

```cpp
namespace entities {
    struct Rock {
        Vec3 position;
        Vec3 velocity;
        Mat3 orientation;
        bool alive = true;
    };

    inline constexpr std::size_t kRockVertexCount = 6;
    inline constexpr std::size_t kRockFaceCount   = 8;
    extern const std::array<Vec3, kRockVertexCount> kRockVertices;

    void step(Rock& rock) noexcept;

    enum class ParticleKind : std::uint8_t {
        Exhaust, Explosion, Bullet, Spark
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

`step(Rock&)` applies gravity + velocity integration.
`step(Particle&)` decrements ttl, applies gravity (kind-dependent),
applies velocity. Exhaust particles do NOT have gravity (so smoke trails
hang in air); other kinds do.

## 7. Determinism

Pure functions of `(in_state)`. PRNG-based spawn happens in Pass 13;
Pass 10 itself has no randomness.

## 8. Out of scope (DEFERRED)

- Random spawn loop (Pass 13).
- Rendering (Pass 13).
- Collision with ship (Pass 11 hooks via classify_collision pattern).
- Rock-vs-rock interaction (NEVER — bbcelite doesn't model it).
- Empirical particle counts/lifetimes → Pass 14.

## 9. Open questions

1. Particle TTL units — frames? Planner default frames (consistent with
   Pass 4 per-frame constants).
2. Exhaust gravity — bbcelite leaves smoke hanging. Planner default:
   exhaust has no gravity; explosion/bullet/spark all do.
3. Rock spawn altitude — bbcelite: "from the sky". Planner default
   y = -50 (Y-DOWN: well above launchpad).

## 10. Clean-room boundary

The 6-vertex octahedron is a planner-default that preserves the
"6 vertices, 8 faces" constraint without copying any specific bbcelite
vertex table. Pass 14 may swap to the bbcelite-shape if desired.

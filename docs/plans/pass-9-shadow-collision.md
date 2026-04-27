# Pass 9 Shadow + Collision — Planner Output (Gate 1)

**Branch:** `feature/pass-9-shadow-collision`
**Date:** 2026-04-27
**Baseline:** 243/243 tests green (Pass 1..8).

## 1. Problem restatement

Two pure functions:

1. `render::project_shadow(world_vertices, out)` — orthographic
   straight-down projection onto terrain altitude.
2. `physics::classify_collision(world_vertices, velocity)` — returns
   `Airborne | Landing | Crashed` based on vertex penetration and
   velocity threshold.

Both stateless. Render side extends `claude_lander_render`; physics side
extends `claude_lander_physics`.

## 2. Locked design decisions

| ID | Decision |
|----|----------|
| **D-ShadowDirection** | Project along `+y` to terrain; per-vertex. |
| **D-ShadowSpan** | Caller supplies output span; sizes must match. |
| **D-CollisionEnum** | `Airborne / Landing / Crashed`. |
| **D-LandingThresholds** | `kSafeContactHeight=0.05f`, `kLandingSpeed=0.01f` (per-axis). |
| **D-PenetrationRule** | Y-DOWN: `world_v.y > altitude(...)` = below ground. |
| **D-XZAtCentroid** | Landing altitude lookup at vertex centroid x/z. |
| **D-Stateless** | No state, `noexcept`, deterministic. |
| **D-NoLut** | `std::abs`, `std::max`. |

## 3. Public API

```cpp
namespace render {
    void project_shadow(
        std::span<const Vec3> vertices_world,
        std::span<Vec3> shadow_out) noexcept;
}

namespace physics {
    inline constexpr float kSafeContactHeight = 0.05f;
    inline constexpr float kLandingSpeed      = 0.01f;

    enum class CollisionState { Airborne, Landing, Crashed };

    CollisionState classify_collision(
        std::span<const Vec3> vertices_world,
        Vec3 velocity) noexcept;
}
```

## 4. Acceptance criteria (24)

### Shadow projection (AC-S01..S05)
- AC-S01 — `project_shadow` for a single vertex `{0, 0, 0}` outputs `{0, altitude(0,0), 0}`.
- AC-S02 — Each output vertex preserves x/z, replaces y with `altitude(x,z)`.
- AC-S03 — Determinism: bit-identical between two runs over the 9 ship vertices.
- AC-S04 — Span size match: `body.size() == shadow_out.size()` honoured.
- AC-S05 — Empty input → empty output (no crash).

### Collision Airborne (AC-S06..S08)
- AC-S06 — Ship high above terrain (all `world_v.y < altitude - 1.0f`): `Airborne`.
- AC-S07 — Velocity high but altitude very high: still `Airborne` (velocity only matters at contact).
- AC-S08 — Single vertex 1 unit above ground, others much higher: `Airborne`.

### Collision Landing (AC-S09..S12)
- AC-S09 — Ship gently touching ground (lowest `world_v.y` within `kSafeContactHeight` of altitude), velocity all axes within `kLandingSpeed`: `Landing`.
- AC-S10 — Lowest vertex exactly at altitude (clearance=0), velocity zero: `Landing`.
- AC-S11 — Velocity exactly at `kLandingSpeed` (boundary): `Landing` (≤ inclusive).
- AC-S12 — `Landing` requires ALL three axes within speed; if one exceeds, `Crashed`.

### Collision Crashed (AC-S13..S17)
- AC-S13 — Any vertex penetrating terrain (`world_v.y > altitude`): `Crashed`.
- AC-S14 — Within contact height but vertical velocity too high: `Crashed`.
- AC-S15 — Within contact height but x-velocity too high: `Crashed`.
- AC-S16 — Within contact height but z-velocity too high: `Crashed`.
- AC-S17 — Multiple vertices penetrating: `Crashed` (single check is enough).

### Determinism (AC-S18..S20)
- AC-S18 — `classify_collision` deterministic over 100 random-but-fixed samples.
- AC-S19 — Order independence: vertices passed in different order yield same result.
- AC-S20 — Velocity sign independence: `velocity={0.005, 0, 0}` and `{-0.005, 0, 0}` give same Landing/Crashed result (uses `|v|`).

### Bug-class fences
- **AC-Spenet** — `world_v.y > altitude` is Crashed (Y-DOWN: below ground). Reversed sign would invert the test.
- **AC-Sclear** — Ship 100 units ABOVE ground = Airborne, NOT Landing. (Catches a missing-height-check bug.)
- **AC-Sspeed** — Velocity per-axis check uses absolute value; sign of velocity doesn't matter.

### Hygiene (AC-S80..S82)
- AC-S80 — `src/render/shadow.{hpp,cpp}` and `src/physics/collision.{hpp,cpp}` exclude raylib/world/entities/`<random>`/`<chrono>`.
- AC-S81 — `claude_lander_render` link list unchanged; `claude_lander_physics` link list unchanged.
- AC-S82 — `static_assert(noexcept(...))` for both public functions.

## 5. Test plan

`tests/test_shadow.cpp` — AC-S01..S05 + part of AC-S80..S82. Tag `[render][shadow]`.
`tests/test_collision.cpp` — AC-S06..S20 + fences + part of AC-S80..S82. Tag `[physics][collision]`.

Tolerances: `kShadowEps = 1e-5f`, `kCollEps = 1e-5f`.

NOTE on AC-S05: empty input → empty output. With `std::span` size 0,
no iterations run. Verify behaviour explicitly.

## 6. CMake plan

- Append `src/render/shadow.cpp` to `claude_lander_render` sources.
- Append `src/physics/collision.cpp` to `claude_lander_physics` sources.
- Append both test files to test sources.
- BUT: physics module currently forbids `world` includes (per
  `physics_no_forbidden_includes`). `classify_collision` needs
  `world::altitude` to compute the ground level.

  **Resolution:** caller (Pass 13) computes `terrain_y = altitude(...)`
  and passes it as a parameter:
  ```cpp
  CollisionState classify_collision(
      std::span<const Vec3> vertices_world,
      float terrain_y_at_centroid,
      Vec3 velocity) noexcept;
  ```
  This keeps physics → core only.

## 7. Definition of Done

- [ ] All 24 ACs green.
- [ ] 243-baseline preserved.
- [ ] No forbidden includes; both modules respect existing tripwires.
- [ ] PR body: `Closes #TBD-pass-9` + DEFERRED + WIRING.

## 8. ADR triggers

Zero. Planner-default thresholds, no new boundary.

## 9. Open questions

- KOQ-1: empirical thresholds → Pass 14.
- KOQ-2: upright check → Pass 14.
- KOQ-3: velocity threshold per-axis vs magnitude → Pass 14.
- KOQ-4: shadow rasterisation → Pass 13.

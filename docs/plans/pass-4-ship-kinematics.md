# Pass 4 Ship + Kinematics — Planner Output (Gate 1)

**Branch:** `feature/pass-4-ship-kinematics`
**Author role:** Planner / Spec & DoD Architect
**Date:** 2026-04-27
**Baseline:** 113/113 tests green (Pass 1 + 2 + 3).

## 1. Problem restatement

Two new modules:
1. `src/entities/ship.{hpp,cpp}` — `Ship { Vec3 position; Vec3 velocity; Mat3 orientation; }` plus 9 body-frame vertex constants. New OBJECT lib `claude_lander_entities`.
2. `src/physics/kinematics.{hpp,cpp}` — pure stateless functions on `Vec3`/`Mat3`. New OBJECT lib `claude_lander_physics`.

## 2. Locked design decisions

| ID | Decision |
|----|----------|
| **D-StepHome** | `step()` lives in `entities/ship.{hpp,cpp}`. `physics/` exposes pure primitives. DAG: `physics → core`, `entities → physics + core`. |
| **D-Thrust** | `enum class ThrustLevel { None, Half, Full };` declared in `physics/kinematics.hpp`. |
| **D-StepOrder** | drag → gravity → thrust → `position += velocity`. AC-K13 pins this. |
| **D-DragSemantics** | `apply_drag(v) → v * kDragPerFrame` uniformly on all axes. |
| **D-GravitySemantics** | `apply_gravity(v) → v + Vec3{0, kGravityPerFrame, 0}` (Y-DOWN; positive y added). |
| **D-ThrustSemantics** | `apply_thrust(v, orient, level) → v + orient.col[1] * factor(level)`. |
| **D-Constants** | `kDragPerFrame = 63/64`, `kFullThrust = 1/2048`, `kHalfThrust = 1/8192` (= Full/4, NOT Full/2), `kGravityPerFrame = 1/4096`, `kFrameRate = 50`, `kFrameDt = 1/50`. |
| **D-PerFrame** | Constants are per-frame; no `dt` multiplication inside kinematics. |
| **D-Vertices** | 9 body-frame vertex constants in `inline constexpr std::array<Vec3, 9> kShipVertices`. |
| **D-FaceCount** | `kShipVertexCount = 9`, `kShipFaceCount = 9`. Face index list deferred to Pass 6. |
| **D-InitialState** | `Ship{}` defaults: `position={0,5,0}`, `velocity={0,0,0}`, `orientation=identity()`. |

## 3. Acceptance criteria

### Drag (AC-K01..K05)
- AC-K01 — `apply_drag({1,0,0})` → `(0.984375, 0, 0)`.
- AC-K02 — `apply_drag({2,4,-8})` → componentwise multiply by `kDragPerFrame`.
- AC-K03 — 64 iterations of `apply_drag` on `{1,0,0}` → `|x| < 0.5f`.
- AC-K04 — `apply_drag({0,0,0})` → bit-equal `{0,0,0}`.
- AC-K05 — `apply_drag({1e6, -1e6, 1e6})` → finite, componentwise multiply.

### Gravity (AC-K06..K08)
- AC-K06 — `apply_gravity({1,2,3})` → `(1, 2+kGravity, 3)` (no x/z mutation).
- AC-K07 — 4096 iterations of `apply_gravity` on `{0,0,0}` → `result.y ≈ 1.0f` within `1e-3f`.
- AC-K08 — `apply_gravity({0,0,0}).y > 0` (Y-DOWN sign).

### Thrust (AC-K09..K12)
- AC-K09 — `apply_thrust(v, orient, None)` → no-op.
- AC-K10 — `apply_thrust({0}, identity(), Full)` → `(0, kFullThrust, 0)`; `Half` → `kFullThrust/4`.
- AC-K11 — `apply_thrust({0}, identity(), Full)` → thrust along `col[1]` (roof).
- AC-K12 — Custom orient with `col[1] = (1,0,0)`, Full → `(kFullThrust, 0, 0)`. Catches "world up" mistakes.

### Step composition (AC-K13..K15)
- AC-K13 — `step(ship={pos=0, vel={1,0,0}, orient=identity()}, Full)`: hand-compute drag→gravity→thrust→`pos += vel`. Verify both `velocity` and `position` within `1e-6f` per component.
- AC-K14 — 1000 `step` calls deterministic (bit-identical to fresh re-run).
- AC-K15 — 1000 frames of `step({pos={0,5,0}, vel=0}, Full)` → terminal `v.y ≈ (kGravity + kFull) * 64 ≈ 0.0469f` within `1e-3f`.

### Ship struct (AC-S01..S04)
- AC-S01 — `Ship{}` defaults match D-InitialState exactly.
- AC-S02 — `kShipVertices.size() == 9 == kShipVertexCount`.
- AC-S03 — All 9 vertices match spec table within `5e-4f`.
- AC-S04 — `kShipFaceCount == 9` (face index list DEFERRED to Pass 6).

### Hygiene (AC-K80..K82, AC-S80)
- AC-K80 — `physics/kinematics.hpp` excludes raylib/world/entities/render/`<random>`/`<chrono>`. `static_assert(!defined(RAYLIB_VERSION))` + CMake grep.
- AC-K81 — `claude_lander_physics` link list = core + warnings only.
- AC-K82 — `apply_*` functions are pure (deterministic; covered by AC-K14).
- AC-S80 — `entities/ship.hpp` excludes world/render/raylib/platform.

## 4. Test plan

- `tests/test_kinematics.cpp` — AC-K01..K15 + AC-K80..K82. Tag `[physics][kinematics]`.
- `tests/test_ship.cpp` — AC-S01..S04 + AC-S80. Tag `[entities][ship]`.

Tolerances: `kKinEps = 1e-6f`, `kKinDecayEps = 1e-3f`, `kVertexEps = 5e-4f`.

## 5. Architecture and contract

**Public API:**
```cpp
namespace physics {
    inline constexpr float kDragPerFrame    = 63.0f / 64.0f;
    inline constexpr float kFullThrust      = 1.0f / 2048.0f;
    inline constexpr float kHalfThrust      = 1.0f / 8192.0f;
    inline constexpr float kGravityPerFrame = 1.0f / 4096.0f;
    inline constexpr int   kFrameRate       = 50;
    inline constexpr float kFrameDt         = 1.0f / 50.0f;

    enum class ThrustLevel { None, Half, Full };

    Vec3 apply_drag   (Vec3 velocity) noexcept;
    Vec3 apply_gravity(Vec3 velocity) noexcept;
    Vec3 apply_thrust (Vec3 velocity, const Mat3& orientation, ThrustLevel level) noexcept;
}

namespace entities {
    inline constexpr std::size_t kShipVertexCount = 9;
    inline constexpr std::size_t kShipFaceCount   = 9;
    extern const std::array<Vec3, kShipVertexCount> kShipVertices;

    struct Ship {
        Vec3 position    { 0.0f, 5.0f, 0.0f };
        Vec3 velocity    { 0.0f, 0.0f, 0.0f };
        Mat3 orientation { /* identity */ };
    };

    void step(Ship& ship, physics::ThrustLevel thrust) noexcept;
}
```

**CMake:**
```cmake
add_library(claude_lander_physics OBJECT src/physics/kinematics.cpp)
target_include_directories(claude_lander_physics PUBLIC src)
target_link_libraries(claude_lander_physics PRIVATE
    claude_lander_core claude_lander_warnings)

add_library(claude_lander_entities OBJECT src/entities/ship.cpp)
target_include_directories(claude_lander_entities PUBLIC src)
target_link_libraries(claude_lander_entities PRIVATE
    claude_lander_core claude_lander_physics claude_lander_warnings)
```

Both linked into `ClaudeLander` and `claude_lander_tests`.

CTest tripwires:
- `physics_no_forbidden_includes` (no raylib/world/entities/render/`<random>`/`<chrono>` in `src/physics/`).
- `entities_no_forbidden_includes` (no raylib/world/render in `src/entities/`).

## 6. Definition of Done

- [ ] All ACs green.
- [ ] 113-test baseline preserved.
- [ ] No raylib/platform `#ifdef`/`<random>`/`<chrono>`/TODO/FIXME in `src/physics/` or `src/entities/`.
- [ ] Two new OBJECT libs wired correctly.
- [ ] Tests-only build green.
- [ ] CTest tripwires registered and passing.
- [ ] PR body: `Closes #TBD-pass-4` + `DEFERRED: empirical thrust/gravity tuning → Pass 14`.

## 7. ADR triggers

Zero new ADRs. Modules follow Pass 2/3 patterns. KOQ-1 (thrust magnitudes) and KOQ-2 (gravity) are spec-defaults; if Pass 14 demands tuning, that ADR lands then.

## 8. Known open questions

- KOQ-1: thrust magnitude — `1/2048` and `1/8192` are spec-defaults; Pass 14 may retune.
- KOQ-2: gravity — `1/4096` is planner default.
- KOQ-3: spawn at `(0, 5, 0)` collides with `terrain::altitude(0,0) = 5.0f` exactly. Pass 9 (collision) handles.
- KOQ-4: position integration uses post-thrust velocity. AC-K13 pins this.
- KOQ-5: Half = Full/4, NOT Full/2. AC-K10 catches the `/2` bug.

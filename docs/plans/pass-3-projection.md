# Pass 3 Projection — Planner Output (Gate 1)

**Branch:** `feature/pass-3-render-projection`
**Author role:** Planner / Spec & DoD Architect
**Date:** 2026-04-27

## 1. Problem restatement and explicit constraints

Implement `src/render/projection.{hpp,cpp}` exposing a clean-room
perspective projection function and a `Camera` struct, plus a new
lightweight `src/core/vec2.hpp` (added because no `Vec2` type exists in
core yet).  Output is a screen pixel coordinate or "culled" signal — no
raylib draw calls, no `world/` dependency, no platform `#ifdef`.

**Hard constraints:**
- `render/` depends on `core/` **only** (no `world/`, no `entities/`).
- Y-DOWN end-to-end (world & raylib 2D screen share polarity).
- Single-flip-point rule re-interpreted per **D-Yflip** below.
- New `claude_lander_render` OBJECT lib, linked into game + tests.
- Tests-only build (`-DCLAUDE_LANDER_BUILD_GAME=OFF`) must compile/link.
- One PR objective — projection only.
- Pass 1 (49 tests) + Pass 2 (26 tests) baseline must remain green.

## 2. Locked planner decisions

| ID | Decision | Rationale |
|----|----------|-----------|
| **D-Yflip** | Projection applies **no y-negation**.  Formula is `y_screen = 64 + y_c / z_c` exactly.  Single-flip-point rule means projection *names* world-vs-screen convention, not that a negation must occur. | World Y-DOWN ≡ raylib 2D screen Y-DOWN ≡ Mode 13 Y-DOWN.  Bolting on `-y/z` "for safety" silently inverts the screen and reproduces the bug class that killed prior iterations.  AC-R20..R22 is the loud guard test. |
| **D-Cull** | Return `std::optional<Vec2>` (`std::nullopt` ⇒ culled). | Standard library leverage; no extra type to maintain; idiomatic null-check. |
| **D-Cam** | `struct Camera { Vec3 position; }` for Pass 3.  No rotation matrix yet (bbcelite camera is fixed). | Spec confirms camera is fixed; Pass 7 reuses this same `project()` with a different `position` per frame. |
| **D-FloatVsFixed** | IEEE-754 `float`. | ARM precision ≤24 bits; float significand is 24 bits → strictly superior. |
| **D-NearFarCull** | `kNearCullZ = 0.01f`; `kFarCullZ = 1e6f`. | Avoids divide-by-near-zero; 1e6 tile-units is well past any visible terrain.  KOQ-1 records the choice. |
| **D-AspectRatio** | No aspect correction.  Single divisor `z` for both axes. | Faithful to original; KOQ-2 records future square-pixel option goes via new ADR. |
| **D-ScreenCenter** | `kScreenCenterX = 160.0f`, `kScreenCenterY = 64.0f`. | Per spec.  64 (not 128) is the bbcelite original. |
| **D-LogicalScreen** | `kLogicalScreenW = 320`, `kLogicalScreenH = 256`.  Constants exposed but not used by Pass 3 to clip — the rasteriser (Pass 6+) clips. | Per spec. |
| **D-Vec2** | Add `src/core/vec2.hpp` with `struct Vec2 { float x, y; }` plus minimal arithmetic + `approx_equal`.  Header-only (no `.cpp`). | Pass 3 needs a `Vec2`; no other module needs it yet.  Header-only keeps `claude_lander_core` translation-unit list unchanged. |

## 3. Acceptance criteria (Given/When/Then)

All tags `[render][projection]`.  No goldens — every expected pixel is
hand-derivable.

### Vec2 + Camera struct (AC-R01..R04)
- **AC-R01** — `Vec2{1.0f, 2.0f}` reads `.x == 1.0f`, `.y == 2.0f`.
- **AC-R02** — `Camera{Vec3{1,2,3}}` reads `position == (1,2,3)`.
- **AC-R03** — `Camera{}` default-constructs `position == (0,0,0)`.
- **AC-R04** — `approx_equal(Vec2{1,2}, Vec2{1,2})` → true; with `(1.001f, 2.0f)` and eps `1e-5f` → false.

### Identity (AC-R05..R07)
- **AC-R05** — `world_point == camera.position` → `nullopt` (z_cam = 0 fails near-cull).
- **AC-R06** — Camera origin, point `(0,0,10)` → `(160, 64)`.
- **AC-R07** — Point `(0, 0, kNearCullZ * 0.5f)` → `nullopt`.

### On-axis projection (AC-R10..R14)
- **AC-R10** — `(0, 0, 10)` → `(160, 64)`.
- **AC-R11** — `(0, 0, 100)` → `(160, 64)` (any positive on-axis z gives screen-centre).
- **AC-R12** — `(0, 0, 1.0f)` → `(160, 64)`.
- **AC-R13** — `(0, 0, 999999.0f)` → `(160, 64)`.
- **AC-R14** — `(0, 0, kFarCullZ * 0.99f)` → returns Some.

### Off-axis projection (AC-R15..R18)
- **AC-R15** — `(100, 0, 10)` → `(170, 64)` (160 + 100/10).
- **AC-R16** — `(0, 50, 10)` → `(160, 69)` (64 + 50/10).
- **AC-R17** — `(0, 5, 10)` → `(160, 64.5)`.
- **AC-R18** — `(20, -30, 10)` → `(162, 61)`.

### Y-flip CRITICAL guard (AC-R20..R22) — bug-class fence
- **AC-R20** — `(0, +10, 10)` (positive y in Y-DOWN world = below midline) → `result->y > kScreenCenterY`.  **A negation flip inverts this test.**
- **AC-R21** — `(0, -10, 10)` → `result->y < kScreenCenterY`.
- **AC-R22** — `(0, +10, 10)` and `(0, -10, 10)` y-deviations from centre are equal in magnitude and opposite in sign.

### Behind/at/near-camera culling (AC-R30..R35)
- **AC-R30** — `(0, 0, -1)` → `nullopt`.
- **AC-R31** — `(0, 0, -0.0001f)` → `nullopt`.
- **AC-R32** — `(0, 0, 0)` → `nullopt`.
- **AC-R33** — `(0, 0, kNearCullZ)` → `nullopt` (`<=` rule).
- **AC-R34** — `(0, 0, kNearCullZ + 1e-4f)` → returns Some.
- **AC-R35** — `(100, 200, kNearCullZ + 1e-4f)` → finite, far from centre.

### Too-far culling (AC-R40..R42)
- **AC-R40** — `(0, 0, 2e6f)` → `nullopt`.
- **AC-R41** — `(0, 0, kFarCullZ)` → `nullopt` (`>=` rule).
- **AC-R42** — `(0, 0, kFarCullZ - 1.0f)` → returns Some.

### Camera offset (AC-R50..R52)
- **AC-R50** — Camera at `(0, 0, -5)`, point `(0, 0, 0)` → z_cam=+5, result `(160, 64)`.
- **AC-R51** — Camera at `(10, 20, -5)`, point `(10, 20, 0)` → result `(160, 64)`.
- **AC-R52** — Camera at `(0, -10, 0)`, point `(0, 0, 10)` → y_cam=+10, z_cam=10, result `(160, 65)`.

### Aspect-ratio invariant (AC-R60..R62)
- **AC-R60** — `(10, 0, 10)` and `(0, 10, 10)` project to equal pixel-distance from centre.
- **AC-R61** — `(100, 0, 10)` distance = `(0, 100, 10)` distance = 10 px.
- **AC-R62** — Per-axis distances symmetric.

### Determinism (AC-R70)
- **AC-R70** — `project((3,7,11), origin-camera)` called 1000× returns bit-identical results.

### Architecture hygiene (AC-R80..R82)
- **AC-R80** — `src/render/projection.hpp` includes no `world/`, `entities/`, or `raylib` header (`static_assert(!defined(RAYLIB_VERSION))` + CMake grep tripwire).
- **AC-R81** — Build with `-DCLAUDE_LANDER_BUILD_GAME=OFF` succeeds; tests link `claude_lander_render` without raylib.
- **AC-R82** — `claude_lander_render` link list = `claude_lander_core` + `claude_lander_warnings` only.

## 4. Test plan

**File:** `tests/test_projection.cpp`.  Tags `[render][projection]`.  No
`[.golden]` — every expected pixel is hand-derived.

**Tolerance constants:**
```cpp
static constexpr float kProjEps     = 1e-5f;
static constexpr float kBoundaryEps = 1e-4f;
```

**Test executable wiring:** add `tests/test_projection.cpp` to
`add_executable(claude_lander_tests ...)` and add `claude_lander_render`
to its `target_link_libraries`.

## 5. Architecture and contract notes

**Files:**
- `src/core/vec2.hpp` — header-only.
- `src/render/projection.hpp` — declares `Camera`, `project`, public constants. Includes `<optional>`, `core/vec3.hpp`, `core/vec2.hpp`.
- `src/render/projection.cpp` — defines `project`. Includes `<cmath>`, `core/vec3.hpp`, `core/vec2.hpp`, `render/projection.hpp`. **No raylib. No platform `#ifdef`.**

**Public API:**
```cpp
namespace render {
    inline constexpr float kScreenCenterX  = 160.0f;
    inline constexpr float kScreenCenterY  =  64.0f;
    inline constexpr float kNearCullZ      = 0.01f;
    inline constexpr float kFarCullZ       = 1.0e6f;
    inline constexpr int   kLogicalScreenW = 320;
    inline constexpr int   kLogicalScreenH = 256;

    struct Camera { Vec3 position; };

    std::optional<Vec2> project(const Vec3& world_point,
                                const Camera& camera) noexcept;
}
```

**CMake addition:**
```cmake
add_library(claude_lander_render OBJECT
    src/render/projection.cpp
)
target_include_directories(claude_lander_render PUBLIC src)
target_link_libraries(claude_lander_render PRIVATE
    claude_lander_core
    claude_lander_warnings
)
```

**Boundary tripwire CTest:**
```cmake
add_test(
    NAME render_no_raylib_or_world_includes
    COMMAND ${CMAKE_COMMAND} -E env bash -c
        "if grep -RnE '^[[:space:]]*#[[:space:]]*include[[:space:]]*[<\"](raylib|rlgl|raymath|world/|entities/)' ${CMAKE_SOURCE_DIR}/src/render/ ; then exit 1 ; else exit 0 ; fi"
)
```

## 6. PR slicing

One PR.  `feature/pass-3-render-projection` → `main`.

**Out of scope:** raylib draw calls (Pass 6), camera-follow logic (Pass 7),
HUD (Pass 12), real-world visual validation (Pass 14, DEFERRED).

## 7. Definition of Done

- [ ] All ACs covered by passing tests.
- [ ] Pass 1 + Pass 2 baseline still green (75/75 + N).
- [ ] `src/render/projection.{hpp,cpp}` contain no raylib include, no `world/`/`entities/` include, no platform `#ifdef`, no TODO/FIXME.
- [ ] `claude_lander_render` is `add_library(... OBJECT ...)` linked only to `claude_lander_core` + `claude_lander_warnings`.
- [ ] Both game (BUILD_GAME=ON) and tests link `claude_lander_render`.
- [ ] Tests-only build green.
- [ ] CTest `render_no_raylib_or_world_includes` registered and passing.
- [ ] PR body: `Closes #TBD-pass-3` + `DEFERRED: real-world visual validation → Pass 14`.
- [ ] **D-Yflip documented in `src/render/projection.hpp` header AND in `docs/adr/0006-pass-3-y-flip-no-negation.md`** with bug-class history.
- [ ] AC-R20..R22 named "bug-class fence" in commit messages or PR description.
- [ ] No `Vec2` outside `src/core/vec2.hpp` (single source of truth).

## 8. ADR triggers

| Trigger | ADR? | File | Rationale |
|---------|------|------|-----------|
| **D-Yflip** | YES | `docs/adr/0006-pass-3-y-flip-no-negation.md` | Re-interprets architecture's "single-flip-point" rule.  Records bug-class history. |
| D-Cull | NO | header comment | Internal API choice. |
| D-Cam | NO | header comment | Forward-compatible. |
| D-FloatVsFixed | NO | header comment | Spec already justifies. |
| D-NearFarCull | NO | header comment naming KOQ-1 | Tunable constants, not architectural. |
| D-AspectRatio | NO (yet) | header comment naming KOQ-2 | ADR-required IF Pass 14 introduces square-pixel option. |
| D-Vec2 | NO | none | Additive header. |

## 9. Known open questions (KOQ)

- **KOQ-1** — `kFarCullZ = 1e6f` chosen, not derived.  Original used fixed-point `0x80000000` ≈ 2.1e9; float equivalent is unit-dependent.  Revisit if Pass 14 surfaces issues.  Constant exposed in header — one-line tune.
- **KOQ-2** — No aspect correction (D-AspectRatio).  Future square-pixel option requires new ADR.
- **KOQ-3** — `kScreenCenterY = 64` (not 128) per spec.  Pass 14 may revisit.
- **KOQ-4** — Near-plane partial-line clipping deferred to Pass 6 rasteriser.
- **KOQ-5** — `Vec2` ships minimal surface; extends additively when later modules need more operators.

## 10. Implementer's pre-flight notes

- **Derive every AC's expected pixel by hand** before writing the assertion.  Do not compute via the implementation — that's circular.
- **AC-R20..R22 are the bug-class fence.** If you ever feel the urge to write `-y_c / z_c` or `kScreenCenterY - ...`, stop, re-read D-Yflip, and run AC-R20..R22 first.
- The header comment in `projection.hpp` **must** contain a paragraph titled "Y-CONVENTION (READ BEFORE EDITING)" pointing to ADR-0006.
- `core/vec2.hpp` is header-only; do NOT add a `vec2.cpp`.
- Do not include `<random>`, `<chrono>`, `<thread>`, or anything non-deterministic.

## Summary

- **AC count:** 32 (R01..R82 across 9 groups).
- **ADR count:** 1 required (`0006-pass-3-y-flip-no-negation.md`).
- **Key design decision:** **D-Yflip** — projection applies NO y-negation; world Y-DOWN ≡ raylib 2D screen Y-DOWN.  AC-R20..R22 is the loud guard.
- **Open questions:** KOQ-1 far-cull tuning, KOQ-2 aspect, KOQ-3 horizon Y, KOQ-4 near-plane clipping, KOQ-5 Vec2 surface.
- **Blockers:** none.  All design decisions locked; new core file is additive header-only.

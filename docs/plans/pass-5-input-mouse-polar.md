# Pass 5 Mouse Polar Input — Planner Output (Gate 1)

**Branch:** `feature/pass-5-input-mouse-polar`
**Author role:** Planner / Spec & DoD Architect
**Date:** 2026-04-27
**Baseline:** 137/137 tests green (Pass 1 + 2 + 3 + 4).

## 1. Problem restatement

Add the mouse → polar → orientation matrix pipeline as a new
`claude_lander_input` OBJECT lib. Pure stateless math: caller supplies
a mouse offset already relative to centre; module returns a
freshly-rebuilt orientation matrix.

Out of scope (DEFERRED):
- Sensitivity multiplier, pitch clamp, yaw modulo (Pass 14).
- Mouse-centre re-capture on focus loss (Pass 14).
- LUT integration with Pass 1 `lookup_tables.hpp` (Pass 14 if profiling).
- Wiring into game loop / `Ship` (Pass 13).

## 2. Locked design decisions

| ID | Decision |
|----|----------|
| **D-PolarLib** | New OBJECT lib `claude_lander_input`. DAG: `input → core`. |
| **D-Stateless** | Every public function `noexcept`, no globals. |
| **D-AngleStateLocation** | `ShipAngles` lives in caller (Pass 13). |
| **D-CentreOwnedByCaller** | Mouse centre owned by caller; module receives `(dx, dy)` already. |
| **D-NoLut** | Use `std::sin`, `std::cos`, `std::atan2`, `std::sqrt`. Pass 14 may revisit. |
| **D-DampRatio** | `damp(prev, input) = 0.5f * prev + 0.5f * input`. |
| **D-NoClamp** | No pitch clamp; no yaw modulo. Pass 14 KOQ. |
| **D-MatrixLayout** | Column-major: `col[0]=nose`, `col[1]=roof`, `col[2]=side`. Per spec table. |
| **D-FullRebuild** | Matrix rebuilt from scratch per call; no incremental composition. |
| **D-AtanSemantics** | `to_polar` uses `std::atan2(dy, dx)`; output `angle ∈ (-π, π]`. |
| **D-NoDeadZone** | No threshold near `(0, 0)`. |
| **D-PolarOffsetOrder** | `PolarOffset { float radius; float angle; }` — order locked. |
| **D-ShipAnglesNames** | `ShipAngles { float pitch; float yaw; }` — pitch first. |
| **D-RoofAtCol1** | `orientation_from_pitch_yaw(0,0).col[1] == (0,1,0)`. AC-I12 fence. |

## 3. Acceptance criteria

### `to_polar` (AC-I01..I05)

- AC-I01 — `to_polar({0,0}) == {0,0}` exactly.
- AC-I02 — Axis-aligned: `(1,0)`→`(1, 0)`, `(0,1)`→`(1, π/2)`, `(-1,0)`→`(1, π)`, `(0,-1)`→`(1, -π/2)`. Within `kPolarEps`.
- AC-I03 — All 4 quadrants: `(1,1)`→`(√2, π/4)`, `(-1,1)`→`(√2, 3π/4)`, `(-1,-1)`→`(√2, -3π/4)`, `(1,-1)`→`(√2, -π/4)`.
- AC-I04 — Large magnitudes finite: `to_polar({1e6, 1e6})` is finite, `radius ≈ √2 · 1e6`, `angle ≈ π/4`.
- AC-I05 — Round-trip: `(radius·cos(angle), radius·sin(angle))` reconstructs `(dx, dy)` for `(3,4)`, `(-7,24)`, `(0.5, -0.866)`.

### `damp` (AC-I06..I09)

- AC-I06 — **Bug-class fence (AC-Idamp)**: `damp(0, 1) == 0.5f` exactly.
- AC-I07 — Identity at fixpoint: `damp(x, x) == x` for `x ∈ {0, 1, -1, 0.5, -π}`.
- AC-I08 — Decay to input: 30 iterations of `damp(prev, 1.0f)` from `prev=0` give `|prev - 1| < 1e-6f`.
- AC-I09 — NaN propagation guard: `damp(NaN, 0)` is NaN; `damp(0, NaN)` is NaN.

### `orientation_from_pitch_yaw` (AC-I10..I16)

- AC-I10 — Identity at zero: `orientation_from_pitch_yaw(0, 0) == identity()` within `kMatEps`.
- AC-I11 — Determinant invariant: `|det(M) - 1| < kMatEps` for `(0,0)`, `(π/4, π/4)`, `(π/3, -π/6)`, `(-π/4, π/2)`.
- AC-I12 — **Bug-class fence (AC-Iy-up)**: `orientation_from_pitch_yaw(0,0).col[1] == (0, 1, 0)` within `kMatEps`.
- AC-I13 — Pure pitch tilts nose: `orientation_from_pitch_yaw(π/2, 0).col[0] == (0, 1, 0)`.
- AC-I14 — **Bug-class fence (AC-Iyaw-roll)**: `orientation_from_pitch_yaw(0, π/2)`: `col[0] == (0, 0, -1)`, `col[1] == (0, 1, 0)`, `col[2] == (1, 0, 0)`.
- AC-I15 — Orthonormal columns at `(π/3, -π/6)`: each column unit-length within `kMatEps`; pairwise dot products `< kMatEps`.
- AC-I16 — Roof tilts forward: at `(π/4, 0)`, `col[1].x = -√2/2`, `col[1].y = √2/2`. Within `kMatEps`.

### `update_angles` + composite (AC-I17..I20)

- AC-I17 — Composes correctly: `update_angles({0,0}, {1,0})` → `{0.5, 0.0}` within `kAngleEps`.
- AC-I18 — Non-trivial prev: `update_angles({0.4, 0.2}, {0,1})` → `{0.7, 0.5*0.2 + 0.5*(π/2)}`.
- AC-I19 — `build_orientation(angles) == orientation_from_pitch_yaw(angles.pitch, angles.yaw)` element-wise exactly.
- AC-I20 — 1000 deterministic iterations: bit-identical between two fresh runs.

### Convergence (AC-I21..I23)

- AC-I21 — Constant offset `{1,0}`, 30 frames → `|pitch - 1| < 1e-6f`, `|yaw - 0| < kAngleEps`.
- AC-I22 — Constant offset `{1,1}`, 30 frames → `|pitch - √2| < 1e-6f`, `|yaw - π/4| < kAngleEps`.
- AC-I23 — 50-frame steady state with `{1,0}`: `|det(M) - 1| < kMatEps` and `|col[1].length() - 1| < kMatEps`.

### Hygiene (AC-I80..I82)

- AC-I80 — `src/input/` excludes raylib/world/entities/render/physics/`<random>`/`<chrono>`. CTest tripwire `input_no_forbidden_includes`.
- AC-I81 — `claude_lander_input` link list = `core` + `warnings` only.
- AC-I82 — All public functions are `noexcept` (verified by `static_assert`).

## 4. Test plan

`tests/test_mouse_polar.cpp` — AC-I01..I23 + AC-I80..I82. Tag `[input][mouse_polar]`.

Tolerances: `kPolarEps = 1e-5f`, `kAngleEps = 1e-6f`, `kMatEps = 1e-5f`.

## 5. Architecture and contract

**Public API:**
```cpp
namespace input {
    struct PolarOffset { float radius; float angle; };
    PolarOffset to_polar(Vec2 offset) noexcept;

    float damp(float prev, float input) noexcept;
    Mat3  orientation_from_pitch_yaw(float pitch, float yaw) noexcept;

    struct ShipAngles { float pitch; float yaw; };
    ShipAngles update_angles(ShipAngles prev, Vec2 mouse_offset) noexcept;
    Mat3       build_orientation(ShipAngles angles) noexcept;
}
```

**Matrix formula (`a=pitch`, `b=yaw`):**
```
        col[0]=nose          col[1]=roof          col[2]=side
row 0   cos(a)·cos(b)        -sin(a)·cos(b)        sin(b)
row 1   sin(a)               cos(a)                0
row 2  -cos(a)·sin(b)        sin(a)·sin(b)         cos(b)
```

**CMake:**
```cmake
add_library(claude_lander_input OBJECT src/input/mouse_polar.cpp)
target_include_directories(claude_lander_input PUBLIC src)
target_link_libraries(claude_lander_input PRIVATE
    claude_lander_core claude_lander_warnings)
```
Link into `ClaudeLander` and `claude_lander_tests`.

CTest tripwire: `input_no_forbidden_includes`.

## 6. Definition of Done

- [ ] All 24 ACs green.
- [ ] 137-test baseline preserved (Pass 1+2+3+4 carry forward).
- [ ] No raylib/platform/`<random>`/`<chrono>`/TODO/FIXME in `src/input/`.
- [ ] `claude_lander_input` OBJECT lib wired correctly.
- [ ] Tests-only build green; full app build green.
- [ ] CTest tripwire registered and passing.
- [ ] PR body: `Closes #TBD-pass-5` + `DEFERRED: sensitivity/clamp/modulo/centre-recapture/LUT → Pass 14` + `WIRING: angle state and mouse polling → Pass 13`.

## 7. ADR triggers

Zero new ADRs expected. Module follows Pass 2/3/4 patterns. Triggers
that would force STOP+ADR-Lite: dependency outside `core`, mutable
state, divergence from the spec matrix formula, raylib in `src/input/`,
LUT introduction (would supersede D-NoLut).

## 8. Known open questions

- KOQ-1: empirical feel — sensitivity, dead-zone deferred to Pass 14.
- KOQ-2: yaw wraparound — atan2 returns `(-π, π]`; multi-frame damping across `±π` seam might glitch. Pass 14 unwraps if needed.
- KOQ-3: pitch clamp — no clamp here; Pass 14 may add `[-π/2, π/2]`.
- KOQ-4: mouse centre semantics (capture-once vs. re-capture-on-focus) — Pass 13 decides.
- KOQ-5: LUT integration — Pass 14 if profiling demands.
- KOQ-6: NaN policy — propagated, not special-cased; Pass 13 platform glue guards if needed.

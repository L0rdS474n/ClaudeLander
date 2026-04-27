# Pass 6 Render Faces — Planner Output (Gate 1)

**Branch:** `feature/pass-6-render-faces`
**Author role:** Planner / Spec & DoD Architect
**Date:** 2026-04-27
**Baseline:** 164/164 tests green (Pass 1+2+3+4+5).

## 1. Problem restatement

Add per-face backface culling and flat-shading lighting math for the
9-triangle ship mesh. The face index list deferred from Pass 4 §4 lands
here as `kShipFaces`. Pure stateless math: caller supplies world-space
vertices + camera position; module returns either a `VisibleFace`
(outward normal + brightness in `[0, 1]`) or `nullopt` (back-face culled).

A convenience `rotate_vertices(body, orientation, translation, world_out)`
helper transforms body-frame vertices to world frame in one pass.

**Module split:** extend the existing `claude_lander_render` OBJECT lib
with `src/render/faces.cpp`. **No new lib.**

**Critical boundary resolution:** the existing
`render_no_raylib_or_world_includes` tripwire forbids `render/` from
including `entities/`. So `ShipFace` POD lives in **`src/core/face_types.hpp`**
(pure type, no behaviour). Both `entities/` and `render/` depend on
`core/`, so this is DAG-clean. The face *list* `kShipFaces` lives in
`entities/ship.hpp` (model topology owned by the model).

Out of scope (DEFERRED):
- Empirical brightness coefficients vs bbcelite footage (Pass 14).
- raylib `Color` palette mapping (Pass 13).
- Static object lighting (Pass 11).
- Shadow rendering (Pass 9).
- Pre-computed normal tables (Pass 14 if profiled).

## 2. Locked design decisions

| ID | Decision |
|----|----------|
| **D-FacesInEntities** | `kShipFaces` array lives in `entities/ship.hpp`. |
| **D-FaceTypeInCore** | `ShipFace` POD lives in `src/core/face_types.hpp` to keep DAG clean. |
| **D-ExtendRenderLib** | New files added to existing `claude_lander_render`. No new lib. |
| **D-RecomputeNormals** | `normalize(cross(v1-v0, v2-v0))`; no ARM hex normal tables. |
| **D-FlatShading** | One brightness per face. |
| **D-BrightnessFormula** | `clamp(0.5 + 0.5*(-normal.y) + 0.1*(-normal.x), 0, 1)`. |
| **D-NoPalette** | `float brightness ∈ [0, 1]`; raylib `Color` mapping deferred. |
| **D-NoLut** | `std::sqrt`, `std::clamp`. |
| **D-CullSense** | Visible iff `dot(normal_world, face_center - camera) < 0`. |
| **D-Stateless** | All public functions `noexcept`, no globals. |
| **D-SpanContract** | `rotate_vertices` requires `body.size() == world_out.size()`. |
| **D-ShipFaceLayout** | `ShipFace { uint8_t v0, v1, v2; uint16_t base_colour; }`. |
| **D-VisibleFaceLayout** | `VisibleFace { uint8_t face_index; Vec3 normal_world; float brightness; }`. |
| **D-RoofUpBrightness** | At `normal_world={0,-1,0}`: brightness ≈ 1.0 (Y-DOWN fence). |
| **D-OutwardNormals** | Counter-clockwise winding viewed from outside. |

## 3. Acceptance criteria (24 total)

### `rotate_vertices` (AC-F01..F05)
- AC-F01 — Identity orient + zero translation = pass-through, exact.
- AC-F02 — Identity orient + translation `{3,-2,7}` = `body+t` element-wise within `kFaceEps`.
- AC-F03 — Pure 90° yaw on `kShipVertices`: lengths preserved within `kFaceEps`.
- AC-F04 — Determinism: bit-identical between two fresh runs.
- AC-F05 — Span contract: `body.size() == world_out.size() == 9` honoured.

### `shade_face` cull + shade (AC-F06..F10)
- AC-F06 — Front-face visible: outward-pointing normal toward camera returns populated optional.
- AC-F07 — Back-face culled: returns `std::nullopt`.
- AC-F08 — Edge-on (`dot == 0`): culled (`>=` test).
- AC-F09 — Returned `normal_world` is unit length within `kFaceEps`.
- AC-F10 — Brightness in `[0, 1]` for all sweeps.

### Brightness fences (AC-F11..F13)
- AC-F11 — **Bug-class fence (AC-F-y-up)**: `normal_world={0,-1,0}` → brightness ≈ 1.0 within `kBrightEps`. Y-up refactor flips sign and fails.
- AC-F12 — `normal_world={0,1,0}` → brightness ≈ 0.0 within `kBrightEps` (clamp at floor).
- AC-F13 — X-tweak sign: `{-1,0,0}` → 0.6, `{1,0,0}` → 0.4 within `kBrightEps`.

### Ship topology (AC-F14..F18)
- AC-F14 — `kShipFaces.size() == 9`.
- AC-F15 — All face indices `< kShipVertexCount`.
- AC-F16 — Distinct indices per face (no degenerate triangles).
- AC-F17 — Recomputed normals are unit length within `kFaceEps`.
- AC-F18 — **Bug-class fence (AC-F-winding)**: face 0 outward-normal test: `dot(n, centroid - body_origin) > 0`.

### Integration (AC-F19..F22)
- AC-F19 — Identity ship at origin, camera `{0,0,-5}`: at least one face visible.
- AC-F20 — After 90° yaw: visible-face SET differs from AC-F19 (set inequality).
- AC-F21 — Visible face count ∈ `[1, 9]` (closed convex hull).
- AC-F22 — **Bug-class fence (AC-F-cull-sense)**: synthetic face normal `{0,0,-1}` toward camera at `{0,0,-5}` is visible; `{0,0,1}` is culled.

### Determinism (AC-F23)
- AC-F23 — 1000-iter deterministic sweep: bit-identical between two fresh runs.

### Hygiene (AC-F80..F82)
- AC-F80 — `src/render/faces.{hpp,cpp}` and `src/core/face_types.hpp` exclude raylib/world/entities/`<random>`/`<chrono>`. Existing `render_no_raylib_or_world_includes` tripwire enforces.
- AC-F81 — `claude_lander_render` link list = `core` + `warnings` (unchanged from Pass 3).
- AC-F82 — All public functions `noexcept` (verified by `static_assert`).

## 4. Public API

`src/core/face_types.hpp` (new, POD only):
```cpp
namespace core {
    struct ShipFace {
        std::uint8_t  v0, v1, v2;
        std::uint16_t base_colour;
    };
}
```

`src/entities/ship.hpp` (extended):
```cpp
namespace entities {
    using ShipFace = core::ShipFace;
    inline constexpr std::array<ShipFace, kShipFaceCount> kShipFaces = {{
        {0, 1, 5, 0x080},
        {1, 2, 5, 0x040},
        {0, 5, 4, 0x040},
        {2, 3, 5, 0x040},
        {3, 4, 5, 0x040},
        {1, 2, 3, 0x088},
        {0, 3, 4, 0x088},
        {0, 1, 3, 0x044},
        {6, 7, 8, 0xC80},
    }};
}
```

`src/render/faces.hpp`:
```cpp
namespace render {
    struct VisibleFace {
        std::uint8_t face_index;
        Vec3         normal_world;
        float        brightness;
    };
    std::optional<VisibleFace> shade_face(
        std::span<const Vec3> vertices_world,
        core::ShipFace face,
        Vec3 camera_position) noexcept;
    void rotate_vertices(
        std::span<const Vec3> body,
        const Mat3& orientation,
        Vec3 translation,
        std::span<Vec3> world_out) noexcept;
}
```

## 5. CMake

Append `src/render/faces.cpp` to existing `claude_lander_render` sources.
Append `tests/test_faces.cpp` to test sources.

No new tripwires; existing `render_no_raylib_or_world_includes` covers.

## 6. Definition of Done

- [ ] All 24 ACs green.
- [ ] 164-test baseline preserved.
- [ ] No raylib/world/entities/`<random>`/`<chrono>`/TODO/FIXME in `src/render/` or `src/core/face_types.hpp`.
- [ ] `ShipFace` POD in `core/face_types.hpp`; `kShipFaces` in `entities/ship.hpp`.
- [ ] Existing render tripwire still green.
- [ ] Tests-only build green; full app build green.
- [ ] PR body: `Closes #TBD-pass-6` + DEFERRED + WIRING.

## 7. Test plan

`tests/test_faces.cpp` — AC-F01..F23 + AC-F80..F82. Tag `[render][faces]`.
Tolerances: `kFaceEps=1e-5f`, `kBrightEps=1e-4f`. No `<random>`, no `<chrono>`.

## 8. ADR triggers

Zero ADRs expected. Triggers: raylib in render, render→entities direct include,
mutable state, formula divergence, ARM hex transcription, reversed cull sign.

## 9. Open questions

- KOQ-1: empirical brightness tuning → Pass 14.
- KOQ-2: precomputed vs recomputed normals → Pass 14 if profiled.
- KOQ-3: static object lighting → Pass 11.
- KOQ-4: 256-palette layout → Pass 14.
- KOQ-5: shadow rendering → Pass 9.
- KOQ-6: `core/face_types.hpp` placement validated; revisit if implementation finds cleaner option.

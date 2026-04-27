# Pass 7 Camera Follow — Planner Output (Gate 1)

**Branch:** `feature/pass-7-camera-follow`
**Date:** 2026-04-27
**Baseline:** 190/190 tests green (Pass 1..6).

## 1. Problem restatement

Stateless follow-camera position. Camera 5 tiles behind ship along z,
matches ship.x, mirrors ship.y, clamped above terrain. No rotation,
no state, no smoothing.

**Module placement:** `src/world/camera_follow.{hpp,cpp}` extends
existing `claude_lander_world`. DAG: `world → core` (unchanged).

## 2. Locked design decisions

| ID | Decision |
|----|----------|
| **D-CameraInWorld** | Lives in `world/`; depends on `world::altitude`. |
| **D-NoCameraTilt** | Returns `Vec3` only. |
| **D-Stateless** | `noexcept`, no globals. |
| **D-GroundClampViaTerrain** | `cam.y >= altitude(cam.x, cam.z) + clearance`. |
| **D-NoLut** | `std::max` only. |
| **D-BackOffsetSign** | `cam.z = ship.z - 5*TILE_SIZE`. AC-Cback fence. |
| **D-ClampNotShipY** | Ground clamp uses terrain at *camera* (x, z), not ship's. |
| **D-NaNPropagate** | NaN propagates; no special case. |

## 3. Public API

```cpp
namespace world {
    inline constexpr float kCameraBackOffset      = 5.0f;
    inline constexpr float kCameraGroundClearance = 0.1f;
    Vec3 follow_camera_position(Vec3 ship_position) noexcept;
}
```

Implementation:
```cpp
const float cam_x = ship.x;
const float cam_z = ship.z - kCameraBackOffset * TILE_SIZE;
const float floor_y = altitude(cam_x, cam_z) + kCameraGroundClearance;
const float cam_y = std::max(ship.y, floor_y);
return {cam_x, cam_y, cam_z};
```

## 4. Acceptance criteria (24)

### Basic positioning (AC-C01..C05)
- AC-C01 — Ship `{0,5,0}`: cam = `{0, max(5, alt(0,-5)+0.1), -5}`.
- AC-C02 — Ship `{10,5,0}`: cam.x=10, cam.z=-5.
- AC-C03 — Ship `{0,5,100}`: cam.z=95.
- AC-C04 — `cam.x == ship.x` for ship.x ∈ {-50,-1,0,1,50} when ship.y high.
- AC-C05 — `cam.z == ship.z - 5*TILE_SIZE` for ship.z ∈ {-50,-1,0,1,50,100}.

### Y-clamp (AC-C06..C09)
- AC-C06 — Ship.y=100 (well above): cam.y = ship.y exactly.
- AC-C07 — Ship.y=-1000: cam.y = altitude(cam.x, cam.z) + clearance.
- AC-C08 — Threshold at floor: cam.y == floor.
- AC-C09 — Threshold − ε: cam.y clamped up to floor.

### Determinism (AC-C10..C14)
- AC-C10 — Two runs over 1000-iter sweep: bit-identical.
- AC-C11 — Pure: same input, identical output.
- AC-C12 — No `<random>`, `<chrono>` in source.
- AC-C13 — 10×10×10 grid sweep: stable.
- AC-C14 — Reverse-order sweep: same set of pairs.

### Integration with Pass 3 projection (AC-C15..C18)
- AC-C15 — Vertex at ship pos `{0,5,0}` projects within ±1 px of `(160, 64)`.
- AC-C16 — Ship `{10,5,0}`: vertex at ship pos still near `(160, 64)`.
- AC-C17 — Ship `{0,20,0}`: cam.y=20, vertex at ship pos near `(160, 64)`.
- AC-C18 — Vertex at `{ship.x+1, ship.y, ship.z}` projects RIGHT of centre.

### Edge cases (AC-C19..C21)
- AC-C19 — Negative ship.z: ship `{0,5,-100}` → cam.z=-105.
- AC-C20 — Ship near terrain: cam.y clamped to floor.
- AC-C21 — NaN propagation: `{NaN,5,0}` → `cam.x` is NaN.

### Bug-class fences
- **AC-Cback** — Ship `{0,5,10}`: cam.z=5 (NOT 15). Sign-flip catch.
- **AC-Cground** — Ship `{0,-1000,0}`: cam.y = floor (NOT ship.y).
- **AC-Cnose** — `static_assert(std::is_same_v<decltype(...), Vec3>)`. No Mat3.

### Hygiene (AC-C80..C82)
- AC-C80 — No raylib/render/entities/input/`<random>`/`<chrono>`/TODO/FIXME.
- AC-C81 — `claude_lander_world` link list unchanged.
- AC-C82 — `static_assert(noexcept(follow_camera_position(Vec3{})))`.

## 5. Test plan

`tests/test_camera_follow.cpp`. Tag `[world][camera_follow]`. Tolerance
`kCamEps = 1e-5f`.

## 6. CMake plan

Append `src/world/camera_follow.cpp` to existing `claude_lander_world`.
Append `tests/test_camera_follow.cpp` to test sources. No new tripwires.

## 7. Definition of Done

- [ ] All 24 ACs green.
- [ ] 190-test baseline preserved.
- [ ] No forbidden includes, no TODO/FIXME.
- [ ] `noexcept` static_assert present.
- [ ] Tests-only build green; full app build green.
- [ ] PR body: `Closes #TBD-pass-7` + DEFERRED + WIRING.

## 8. ADR triggers

Zero expected. Triggers: smoothing buffer, return-with-Mat3, dependency
on render/entities/input/raylib, ground-clamp-using-ship-xz, LUT,
camera-in-front-of-ship.

## 9. Open questions

- KOQ-1: empirical clearance value (`0.1`) → Pass 14.
- KOQ-2: smoothing across frames → Pass 14.
- KOQ-3: ship-clips-terrain → physics/collision pass.
- KOQ-4: NaN guard at platform glue level → Pass 13.

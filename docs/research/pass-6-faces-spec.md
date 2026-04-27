# Pass 6 Render Faces — Mathematical Specification

> Gate 1.5 research output for `feature/pass-6-render-faces`.
> Prose-only behavioural spec from <https://lander.bbcelite.com>.
> No ARM code transcribed.

## Sources read

- <https://lander.bbcelite.com/deep_dives/drawing_3d_objects.html> —
  pipeline overview, backface cull, lighting model.
- <https://lander.bbcelite.com/source/main/variable/objectplayer.html>
  — 9-vertex / 9-face ship blueprint with pre-computed normals.
- <https://lander.bbcelite.com/deep_dives/drawing_the_landscape.html> —
  landscape rendering: all tiles drawn unconditionally.
- <https://lander.bbcelite.com/deep_dives/placing_objects_on_the_map.html>
  — static vs rotating objects.
- <https://lander.bbcelite.com/source/main/subroutine/drawobject_part_1_of_5.html>
  — DrawObject pipeline.

## 1. Backface culling

### Rotating objects (ship, rocks)

A face is **visible** iff
```
dot(face_normal_world, camera_to_face_center) < 0
```
where `camera_to_face_center = face_center_world - camera.position` and
`face_normal_world = orientation * face_normal_body`.

When the dot product is `>= 0` the face is culled (not rendered).
Bbcelite: "the face is facing away from us and is not visible".

The test runs in **world space** after applying the rotation matrix to
the body-frame normals.

### Static objects (trees, buildings, gazebos)

All faces drawn unconditionally — no backface cull applied. Bbcelite is
explicit: "all their faces are always drawn".

### Landscape tiles

All 12×10 tile triangles drawn regardless of normal orientation. Player
flying low can see undersides; culling them would punch holes. Drawn
back-to-front for depth ordering (Pass 8 bin-sorter).

### Special attributes

No per-face `back_face` override flag in bbcelite prose. Visibility is
purely geometric.

## 2. Lighting model

Per-face flat shading. Each visible face picks one brightness value
based on the orientation of its **rotated normal**.

### Brightness components

Bbcelite documents two contributions:

1. **Y-component (primary)** — "brightness is larger when the face is
   pointing more directly upwards". Y-DOWN world: roof-up faces have
   `normal.y < 0`, so the implementation must invert the sign or use
   `-normal.y` so "up" maps to brighter.

2. **X-component (tweak)** — "depending on the (potentially rotated)
   face normal's x-coordinate, to simulate the light source being
   slightly to the left". Faces pointing left = brighter; right = darker.

### Brightness formula (planner default)

Bbcelite does not state closed-form coefficients. Planner default for
Pass 6:
```
brightness = clamp(base + weight_y * (-normal.y) + weight_x * (-normal.x), 0, 1)
```
with `base = 0.5f`, `weight_y = 0.5f`, `weight_x = 0.1f`. The negative
signs convert from Y-DOWN world frame ("up" = `-y`) and from "light
slightly to the left" ("brighter" = `-x`). `Empirical tuning DEFERRED
to Pass 14`.

### Light direction

Implicit. Equivalent to a fixed light vector with `(x, y) = (-1, -1)`
(z ignored). Not parametric.

### Palette and quantization

Original Lander runs Mode 13 (320×256, 256-colour palette). Each face
references a base colour; brightness modulates the colour index inside
a contiguous palette range. The C++ port stores brightness as a `float`
in `[0, 1]` and converts to a raylib `Color` at render time. The
exact 256-palette layout is `DEFERRED to Pass 14`.

## 3. Ship faces (9 triangles)

### Vertex table (Pass 4 carry-over)

| # | x | y | z |
|---|------|------|------|
| 0 |  1.000 |  0.3125 |  0.500 |
| 1 |  1.000 |  0.3125 | -0.500 |
| 2 |  0.000 |  0.625  | -0.931 |
| 3 | -0.099 |  0.3125 |  0.000 |
| 4 |  0.000 |  0.625  |  1.075 |
| 5 | -0.900 | -0.531  |  0.000 |
| 6 |  0.333 |  0.3125 |  0.250 |
| 7 |  0.333 |  0.3125 | -0.250 |
| 8 | -0.800 |  0.3125 |  0.000 |

### Face index triples

From bbcelite `objectPlayer` blueprint:

| Face | v0 | v1 | v2 | base colour (hex citation) |
|------|----|----|----|----------------------------|
| 0 | 0 | 1 | 5 | 0x080 |
| 1 | 1 | 2 | 5 | 0x040 |
| 2 | 0 | 5 | 4 | 0x040 |
| 3 | 2 | 3 | 5 | 0x040 |
| 4 | 3 | 4 | 5 | 0x040 |
| 5 | 1 | 2 | 3 | 0x088 |
| 6 | 0 | 3 | 4 | 0x088 |
| 7 | 0 | 1 | 3 | 0x044 |
| 8 | 6 | 7 | 8 | 0xC80 |

### Winding

Counter-clockwise when viewed from outside the body = front-facing.
Normals point outward.

### Pre-computed face normals

Bbcelite stores 32-bit fixed-point (8.24) normals per face. Planner
recommends **recomputing normals from vertex order** in Pass 6 via
`normal = normalize(cross(v1 - v0, v2 - v0))`; this avoids transcribing
ARM hex tables and is mathematically equivalent given consistent
winding. `Pass 14 may switch to pre-computed normals if profiling
demands.`

### C++ representation

```cpp
struct ShipFace {
    std::uint8_t v0, v1, v2;
    std::uint16_t base_colour;
};
inline constexpr std::array<ShipFace, 9> kShipFaces = {{
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
```

## 4. Public API recommendation

```cpp
namespace render {
    struct VisibleFace {
        std::uint8_t face_index;
        Vec3 normal_world;
        float brightness;   // [0, 1]
    };

    // Cull and shade a single face. Returns nullopt if back-facing.
    std::optional<VisibleFace> shade_face(
        std::span<const Vec3> vertices_world,
        ShipFace face,
        Vec3 camera_position) noexcept;

    // Convenience: rotate body-frame vertices into world frame.
    void rotate_vertices(
        std::span<const Vec3> body,
        const Mat3& orientation,
        Vec3 translation,
        std::span<Vec3> world_out) noexcept;
}
```

## 5. Determinism

Pure function of `(vertices_world, face, camera_position)` for cull +
shade; pure function of `(body, orientation, translation)` for rotate.
No PRNG, no clock. Pass 6 ACs include a 1000-iteration determinism loop.

## 6. Open questions / DEFERRED

1. **Exact brightness coefficients** — planner default `base=0.5,
   weight_y=0.5, weight_x=0.1`. Empirical match against bbcelite
   footage in Pass 14.
2. **Pre-computed vs. recomputed normals** — Pass 6 uses recomputed
   from cross-product to stay clean-room. Pass 14 may swap.
3. **Static object lighting** — trees/buildings have no rotation.
   Defer until Pass 11 lands.
4. **Palette layout** — defer to Pass 14.
5. **Shadow rendering** — Pass 9.

## 7. Clean-room boundary

The scout read prose at the listed bbcelite pages and the `objectPlayer`
variable page (which lists vertex/face structure as a labelled table,
not as ARM listing). Hex constants `0x080`, `0x040`, `0x088`, `0x044`,
`0xC80` are quoted verbatim as base-colour citations. Normal hex tables
were inspected but **not transcribed** — Pass 6 recomputes normals from
vertex winding, so no ARM normal table is copied into the codebase.

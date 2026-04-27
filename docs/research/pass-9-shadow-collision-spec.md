# Pass 9 Shadow + Collision — Mathematical Specification

> Gate 1.5 research output for `feature/pass-9-shadow-collision`.
> Prose-only behavioural spec from <https://lander.bbcelite.com>.

## Sources read

- <https://lander.bbcelite.com/deep_dives/collisions_and_bullets.html>
- <https://lander.bbcelite.com/deep_dives/drawing_3d_objects.html>

## 1. Shadow projection

Each ship vertex projects straight DOWN onto the terrain along the
y-axis. Orthographic, light-from-above:

```
shadow_v.x = world_v.x
shadow_v.z = world_v.z
shadow_v.y = altitude(world_v.x, world_v.z)
```

Per-vertex (each face's three vertices project independently). The
resulting triangles use the same face indices as the body mesh.

## 2. Shadow culling

Bbcelite culls back-facing shadows for rotating objects (ship, rocks):
if the **rotated face normal's y-component is downward** (negative in
Y-DOWN convention is "up"; positive is "down"), the face is on the
ship's underside and its shadow would land in the wrong place.

For the C++ port: skip shadow if `normal_world.y > 0` (Y-DOWN: face
points down, i.e. into the ground — its shadow would be inside the
hull). Static objects (trees, buildings) skip culling entirely; Pass 9
covers only the ship.

## 3. Shadow color

Solid black. No shading, no tone blending.

## 4. Body-vs-terrain collision

For each ship body vertex:
1. Transform to world: `world_v = ship.position + ship.orientation * body_v`.
2. Compare to ground: `ground = altitude(world_v.x, world_v.z)`.
3. **In Y-DOWN**: if `world_v.y > ground` then the vertex is BELOW
   ground level — the body has clipped terrain.

This is the canonical shadow-penetration test.

## 5. Landing detection

A landing is permitted iff:

```
ship_low_y     = max y-component of all transformed vertices  (Y-DOWN: lowest)
clearance      = altitude(ship.position.x, ship.position.z) - ship_low_y
landing_ok     = clearance >= 0
                 && clearance <= kSafeContactHeight
                 && |velocity.y| <= kLandingSpeed
                 && |velocity.x| <= kLandingSpeed
                 && |velocity.z| <= kLandingSpeed
```

Constants (planner defaults):

| name                  | value  | rationale |
|-----------------------|--------|-----------|
| `kSafeContactHeight`  | 0.05f  | tiles; "very close to ground but not penetrating" |
| `kLandingSpeed`       | 0.01f  | tiles/frame; matches Pass 4 thrust scale |

`Pass 14 may retune empirically against bbcelite footage.`

## 6. Collision result type

```cpp
enum class CollisionState {
    Airborne,           // no contact
    Landing,            // touched, within velocity threshold
    Crashed,            // vertex penetrated terrain or velocity too high
};
```

## 7. Public API recommendation

```cpp
namespace render {
    // Project body-frame vertices to a flat shadow mesh.
    // Output buffer must be sized to body.size().
    void project_shadow(
        std::span<const Vec3> vertices_world,
        std::span<Vec3> shadow_out) noexcept;
}

namespace physics {
    inline constexpr float kSafeContactHeight = 0.05f;
    inline constexpr float kLandingSpeed      = 0.01f;

    enum class CollisionState { Airborne, Landing, Crashed };

    // Test a ship configuration against terrain.
    CollisionState classify_collision(
        std::span<const Vec3> vertices_world,
        Vec3 velocity) noexcept;
}
```

`render::project_shadow` only needs world-space vertices; it does not
own the rotation. Caller (Pass 13) owns body→world transform.

`physics::classify_collision` reads the world-space vertices, computes
the lowest one (max y in Y-DOWN), checks against `altitude` at the
mesh-centroid x/z, and combines with velocity.

## 8. Out of scope (DEFERRED)

- Object collision (trees, buildings) → Pass 11.
- Rock collision → Pass 10.
- Shadow rasterisation / draw order → Pass 13.
- Bullet collision → Pass 11.
- Empirical landing thresholds → Pass 14.

## 9. Open questions

1. Reference point for "ship position" — centroid? vertex 5? Pass 4
   uses `ship.position` as a free Vec3 unrelated to vertices. Pass 9
   uses world-space vertices.
2. Should `classify_collision` consider rotation (a ship inverted over
   the launchpad)? Planner default: no — landing requires the lowest
   vertices be the gear. Pass 14 may add an "upright" check.
3. Velocity threshold: bbcelite quotes a single landing-speed value;
   we apply it per-axis as a conservative simplification. Pass 14
   may switch to magnitude.

## 10. Clean-room boundary

Behavioral facts only. No ARM hex constants transcribed. The
landing-speed and undercarriage thresholds are planner defaults; their
empirical values are deferred to Pass 14.

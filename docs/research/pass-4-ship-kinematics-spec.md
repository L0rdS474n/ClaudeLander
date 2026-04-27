# Pass 4 Ship + Kinematics — Mathematical Specification

> Gate 1.5 research output for `feature/pass-4-ship-kinematics`.
> Prose-only behavioural spec from <https://lander.bbcelite.com>.
> No ARM code transcribed.

## Sources read

- <https://lander.bbcelite.com/deep_dives/the_main_loop.html> — kinematics step
  ordering inside MoveAndDrawPlayer.
- <https://lander.bbcelite.com/deep_dives/flying_by_mouse.html> — thrust applied
  along the ship's roof vector (negation of exhaust direction).
- <https://lander.bbcelite.com/deep_dives/the_lander_objects.html> — 9-vertex,
  9-face ship mesh.
- <https://lander.bbcelite.com/deep_dives/drawing_3d_objects.html> — rotation /
  orientation pipeline.
- <https://lander.bbcelite.com/deep_dives/collisions_and_bullets.html> — landing
  speed thresholds and safe-height constants.

## Drag model

**Per-frame drag: `kDragPerFrame = 63.0f / 64.0f = 0.984375f`.**

Each axis of velocity is multiplied by this factor every frame BEFORE thrust
and gravity are applied:

```
v_after_drag = v_before * 0.984375f       // applied to vx, vy, vz
```

This is the float-equivalent of the ARM `v - (v >> 6)` shift-and-subtract,
i.e. losing 1/64 of velocity per frame to "air resistance".

## Thrust model

The original applies thrust along the ship's **roof vector** (the body-frame
"up", which points opposite the exhaust plume).  Three button states:

| Button | Thrust factor (planner default) |
|---|---|
| left mouse (full)    | `kFullThrust = 1.0f / 2048.0f`   per frame |
| middle mouse (half)  | `kHalfThrust = 1.0f / 8192.0f`   per frame (1/4 of full) |
| nothing pressed      | 0 |

Float magnitudes are derived from the ARM divisor `2048` (a `>> 11` shift in
fixed-point); the half-thrust factor is one-quarter of full per the deep-dive.
**Exact numerical match with the original is DEFERRED** — Pass 14 e2e will
empirically tune these against bbcelite screenshots/video.

Thrust application:
```
v += roof_vector * thrust_factor       // roof_vector is column 1 of orientation matrix
```

In the project's `Mat3` (Pass 1) layout, `col[1]` is the roof vector.

## Gravity

Gravity is a **constant downward acceleration** applied to the y-axis of
velocity each frame.  The bbcelite prose confirms it is a constant, but does
not state the numerical value.

**Planner default: `kGravityPerFrame = 1.0f / 4096.0f ≈ 2.44e-4`** in
Y-DOWN units (positive y = downward, so gravity adds positive y-velocity).
This is one order of magnitude smaller than `kFullThrust`, matching the
"thrust easily overcomes gravity" feel of the original.

**Empirical validation DEFERRED to Pass 14.**

## Ship mesh (9 vertices, 9 faces in ship-local frame)

Vertices in the ship's local body frame (nose along +x, roof along +y in
ship-local-Y-DOWN, side along +z).  Hex constants from bbcelite are quoted
as direct citations of the prose (not copied as code) and translated to
floats by dividing by `0x01000000 = 16777216` (the ARM 8.24 fixed-point
scale used throughout Lander):

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

Faces (9 triangles) — index triples deferred to a Pass 6 (`render/faces`)
research lookup.  Pass 4 only ships the vertex list and a face-count constant.

## Initial state

bbcelite prose does not state the spawn position numerically.  Planner default:

| field | value | source |
|---|---|---|
| `position`     | `Vec3{0.0f, 5.0f, 0.0f}` | tile (0, 0) altitude `LAND_MID_HEIGHT` from Pass 2 |
| `velocity`     | `Vec3{0.0f, 0.0f, 0.0f}` | starts at rest |
| `orientation`  | `Mat3::identity()`        | nose along +x, roof along +y, side along +z |

This places the ship at the (0, 0) tile corner at altitude 5.0 with zero
velocity and identity orientation.  Empirical adjustment DEFERRED to Pass 14.

## Frame rate

**`kFrameRate = 50` Hz; `kFrameDt = 1.0f / 50.0f = 0.02f` seconds.**

Pass 4's kinematics step is **per-frame**, not per-second — the drag, thrust,
and gravity constants above are already "per frame".  No `dt` multiplication
inside the kinematics math.  The renderer will run at 50 Hz on PAL hardware;
modern displays at 60 Hz will look 20% faster unless Pass 13 (game loop)
clamps the update rate.  That clamp is OUT OF SCOPE for Pass 4.

## Determinism

The kinematics function is **deterministic** given `(state_in, inputs_in)`.
No PRNG, no clock, no global state.  The same `(position, velocity,
orientation, thrust_button, gravity_enabled)` tuple always produces the same
`(position', velocity')`.  Pass 4 ACs include a 1000-iteration determinism
loop.

## Open questions / DEFERRED

1. **Exact thrust magnitude scaling** — `1/2048` is the ARM divisor; the float
   translation 1/2048 is reasonable but Pass 14 visual comparison may demand
   re-scaling.
2. **Exact gravity constant** — `1/4096` is a planner default; bbcelite prose
   does not state the value.
3. **Initial spawn state** — planner default is `(0, 5, 0)` with zero velocity
   and identity orientation; bbcelite does not pin this down.
4. **Face index list (9 triangles)** — deferred to Pass 6 (`render/faces`).
5. **Lander-coordinates-to-world-units mapping** — the spec uses tile-units
   throughout (Pass 2 locked `TILE_SIZE = 1.0f`); the ARM 8.24 fixed-point
   conversion is consistent.

## Clean-room boundary

The scout read prose at the listed bbcelite pages and translated formulae,
constants, and vertex tables into mathematical form above.  Hex values
(`0x01000000`, `0x00500000`, etc.) are quoted as direct citations of
bbcelite's prose tables only; their float-equivalents (`0.3125`, `1.0`, ...)
are computed by simple division by `0x01000000`, not transcribed from any
ARM source.  No mnemonics, register names, or instruction sequences appear.

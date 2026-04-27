# Pass 3 Projection — Mathematical Specification (research, not code)

> Produced by Gate 1.5 (research-evidence-scout) on the
> `feature/pass-3-render-projection` branch.  Prose-only specification of
> the behavioural source <https://lander.bbcelite.com>.  No ARM code,
> hex listings, or mnemonics were transcribed verbatim.

## Sources read

| Source | Contribution |
|--------|--------------|
| <https://lander.bbcelite.com/deep_dives/camera_and_landscape_offset.html> | Camera offset (5 tiles behind ship, z-axis); camera role; landscape offset (separate concept). |
| <https://lander.bbcelite.com/source/main/subroutine/projectvertexontoscreen.html> | Core projection formula: Screen X = 160 + x/z; Screen Y = 64 + y/z; near-plane culling at z < 0x80000000; fixed-point arithmetic; wide-viewing-angle paths. |
| <https://lander.bbcelite.com/deep_dives/drawing_the_landscape.html> | Landscape grid structure (13x11 corners); row-by-row drawing algorithm. |
| <https://lander.bbcelite.com/deep_dives/screen_memory_in_the_archimedes.html> | Mode 13: 320x256 pixels, 256 colours; screen origin (0,0) at top-left; each pixel = one byte. |
| <https://lander.bbcelite.com/deep_dives/drawing_3d_objects.html> | Object drawing pipeline: camera-relative coords → rotation → vertex projection → face visibility → drawing. |

## Camera model

The camera is positioned **5 tiles behind the ship along the z-axis**.
It has **no fixed height (y) offset** — the camera's y-coordinate equals
the ship's current y (clamped against the terrain so the camera does not
sink into the ground when the ship lands).

The camera orientation is **fixed**: it does not rotate with the ship.
It always looks in the positive-z direction; the "behind and slightly
above" feel comes from the ship's flight envelope and the projection,
not from any camera tilt.

**Transform pipeline:**

1. Vertex in world space: `(x_w, y_w, z_w)`.
2. Subtract camera position: `(x_c, y_c, z_c) = (x_w - x_cam, y_w - y_cam, z_w - z_cam)`.
3. No camera rotation is applied (camera is fixed).
4. Perspective divide → screen coordinates.

## Constants

| name | value | unit | source URL/anchor | notes |
|---|---|---|---|---|
| CAMERA_BACK_OFFSET | 5 | tiles (z behind ship) | camera_and_landscape_offset | "five tiles into the screen along the z-axis from the ship's highest flying position." |
| CAMERA_HEIGHT_OFFSET | 0 | tiles (y) — **dynamic** | camera_and_landscape_offset | No fixed y-offset; camera y = ship y (clamped). Treated as input parameter, not a `projection.cpp` constant. |
| SCREEN_CENTER_X | 160 | px | projectvertexontoscreen | `Screen X = 160 + x/z`. |
| SCREEN_CENTER_Y | **64** | px | projectvertexontoscreen | **NOT 256/2 = 128.** Original formula explicitly uses 64. Likely a viewport-within-screen choice (HUD reservation in upper region or horizon placement). |
| LOGICAL_SCREEN_W | 320 | px | screen_memory_in_the_archimedes | Mode 13. |
| LOGICAL_SCREEN_H | 256 | px | screen_memory_in_the_archimedes | Mode 13. |
| NEAR_CULL_Z | `>= 0x80000000` (signed-32 flip threshold) | fixed-point | projectvertexontoscreen | Vertices with `z_cam <= 0` are behind the camera; vertices crossing `0x80000000` are too far. Both are culled. |
| ASPECT_RATIO_HANDLING | SINGLE_FL | (design) | projectvertexontoscreen | No separate focal lengths for x and y; the divisor `z` is shared. The 320:256 (5:4) aspect is **not** corrected. Replicate as-is. |

## Projection pipeline (prose)

```
Input:  vertex (x_w, y_w, z_w) in world space
        camera position (x_cam, y_cam, z_cam) in world space

Step 1  Camera-relative coordinates:
        x_c = x_w - x_cam
        y_c = y_w - y_cam
        z_c = z_w - z_cam

Step 2  Cull tests (return CULLED on either):
        z_c <= 0                       (vertex behind camera)
        |z_c| >= NEAR_CULL_Z bound     (vertex too far)

Step 3  Perspective divide → screen pixels:
        x_screen = SCREEN_CENTER_X + (x_c / z_c)
        y_screen = SCREEN_CENTER_Y + (y_c / z_c)

Step 4  Rasteriser later clips to [0, 320) x [0, 256).
```

The original ARM uses a 9-10-bit shift-and-subtract division for the
common case `|x|, |y| <= z` and an 8-bit division path for "wide" cases.
For the C++ port, IEEE-754 `float` offers ~24 bits of significand, well
beyond ARM precision, so a plain `float`-divide matches functional
behaviour without bit-exact replication.

## Y-flip — single point of negation

**Critical finding:** the original ARM **does NOT negate y** in the
projection.  Both the ARM world and the Mode-13 screen treat y-down as
positive (top-left origin), so the formula is plain `64 + y/z`, no flip.

For the C++ port:

* World space is Y-DOWN per `docs/ARCHITECTURE.md` (Pass 0/1/2 docs).
* raylib's **2D rendering** uses (0, 0) at top-left with y growing
  downward — **Y-DOWN**, identical to Mode 13 and to our world.
* raylib's **3D rendering** is Y-UP, but Pass 3 emits 2D screen pixels;
  it does not use raylib's 3D pipeline.

**Therefore: no negation is required to convert world Y-DOWN to raylib
2D screen Y-DOWN.**  The ARM formula maps directly:

```
y_screen = SCREEN_CENTER_Y + (y_c / z_c)
```

The `docs/ARCHITECTURE.md` rule "Y-flip happens at exactly one place,
the projection function" must therefore be re-read as "the *projection
function* is the only place where the world-vs-screen y convention is
even mentioned" — not as "a negation must occur".  Concretely the
projection.cpp **declares** the convention and **asserts** that no
other module flips y.  Planner Gate 1 must lock this interpretation
explicitly so the implementer does not bolt on a spurious negation
"for safety".

## Behind-camera culling rule

* `z_c <= 0` → vertex is behind camera → CULLED.
* `z_c >= NEAR_CULL_Z` → vertex is too far → CULLED.

The original ARM signals "not drawable" via the carry flag and a
specific output pattern that downstream draw routines check.  For
C++, the cleanest equivalents are either:

* return an `std::optional<Vec2>` (`std::nullopt` = culled), or
* return a `ProjectedPoint { Vec2 pos; bool visible; }` struct.

Planner picks one.  The tests will exercise both behind-camera and
too-far-away vertices.

## Aspect ratio

The original uses a single divisor `z` for both x and y.  The 320:256
(≈ 1.25:1) screen aspect is **not** corrected.  A vertex at
`(100, 0, 10)` and another at `(0, 100, 10)` project to the same pixel
distance from screen-centre, even though screen pixels are not square
in physical units.

To match the original behaviour exactly, **do not** apply per-axis
scaling.  If a future pass wants square-pixel projection, it lands as
a renderer-level option, not in Pass 3.

## Determinism

The projection is deterministic given `(vertex, camera)` only.  No
randomness, no time, no global state.  A canonical implementation is
trivially reproducible across calls.

## Open questions / DEFERRED

1. **Camera y-offset is dynamic, not constant** — `projection.cpp` should
   accept the camera as an input (a `Camera` struct or a `Vec3`-plus-yaw
   parameter), not bake the camera y in as a constant.  Planner default:
   pass `camera_pos: Vec3` and (eventually) a rotation matrix from the
   ship-follow logic in a later pass.  For Pass 3, the camera passed in
   is "raw" — the orientation rotation is *not yet* exercised because
   the camera in this game is fixed (no roll, no tilt).  Pass 7
   (`camera_follow`) will still use this same projection function,
   feeding it a slightly different camera position frame-by-frame.

2. **`SCREEN_CENTER_Y = 64` rationale** — the original chose 64, not
   128.  This visually pushes the horizon up to the top quarter of the
   screen, leaving the bottom three-quarters for ground.  Likely
   intentional artistic choice.  Planner default: ship the value 64 as
   the original specifies; if visual correctness in Pass 14 demands
   `128`, revisit with an ADR.

3. **Culling return shape** — `optional<Vec2>` vs.
   `ProjectedPoint { Vec2; bool }`.  Planner picks; both are correct
   clean-room equivalents of the ARM carry-flag signal.

4. **Floating-point vs. fixed-point** — IEEE-754 `float` exceeds the
   original ARM precision; planner default: use `float`.  A bit-exact
   match is unnecessary unless Pass 14 surfaces a visible drift.

5. **Viewport clipping** — `projection.cpp` returns raw screen pixels;
   the rasteriser (Pass 6+ `render/faces.cpp`) clips to
   `[0, 320) x [0, 256)`.  Pass 3 does not clip; it only culls behind-
   and beyond-camera vertices.

## Clean-room boundary

The scout read the listed bbcelite pages as **prose specification only**.
No ARM source listings, mnemonics, register names, or instruction
sequences were copied verbatim into this document.  Hex constants are
quoted as direct citations of the source's prose tables (`0x80000000`)
to anchor the planner's discussion of the cull threshold; their
floating-point translation is left as an explicit planner decision.

The projection formula `Screen X = 160 + x/z` and `Screen Y = 64 + y/z`
is paraphrased plain math, not transcribed code.

# Pass 5 Mouse Polar Input — Mathematical Specification

> Gate 1.5 research output for `feature/pass-5-input-mouse-polar`.
> Prose-only behavioural spec from <https://lander.bbcelite.com>.
> No ARM code transcribed.

## Sources read

- <https://lander.bbcelite.com/deep_dives/flying_by_mouse.html> — main
  spec for mouse → polar → rotation pipeline.
- <https://lander.bbcelite.com/deep_dives/drawing_3d_objects.html> — column-
  major orientation matrix; nose/roof/side semantics.
- <https://lander.bbcelite.com/source/main/subroutine/calculaterotationmatrix.html>
  — CalculateRotationMatrix prose (storage order, full rebuild each frame).

## Mouse coordinate system

- The cursor's **centre point** is captured once at game start and stored
  as ship state.  All input is **relative offset** `(dx, dy)` from that
  centre.
- The original does **not** auto-recentre the cursor between frames.  The
  player keeps the physical mouse near the centre to fly straight.
- Returning the cursor to the stored centre nulls the input and levels
  the ship.

## Polar conversion

The relative offset `(dx, dy)` is decomposed into:

```
distance = sqrt(dx*dx + dy*dy)        // squareRootTable lookup
angle    = atan2(dy, dx)              // arctanTable lookup with quadrant fix
```

The original makes both coordinates positive before the arctan lookup, then
adjusts the final angle based on the original signs to land in the correct
quadrant.  No explicit dead-zone is documented; the prose comments that the
exact centre is "very difficult to control" but does not mask a threshold.

## Pitch + yaw mapping

Two persistent float angles in ship state:

| state | meaning | derived from |
|---|---|---|
| `shipPitch`     | nose-up / nose-down tilt | `distance` |
| `shipDirection` | yaw (heading)            | `angle`    |

**No roll.**  Bbcelite is explicit: only pitch and yaw are exposed to the
player.

### Damping (per-frame smoothing)

Each frame, the angles are blended 50/50 with the new polar reading:

```
shipPitch     = shipPitch     - (shipPitch     - distance) / 2
shipDirection = shipDirection - (shipDirection - angle)    / 2
```

Equivalently:
```
shipPitch     = 0.5f * shipPitch     + 0.5f * distance
shipDirection = 0.5f * shipDirection + 0.5f * angle
```

Bbcelite: "Rather than immediately adopting new values, damping smooths
control responsiveness."

## Rotation matrix construction

The full 3×3 orientation matrix is **rebuilt from scratch every frame**
from the damped `(shipPitch, shipDirection)` pair.  No incremental
composition.

Letting `a = shipPitch`, `b = shipDirection`, the column-major matrix is:

```
        col[0]=nose          col[1]=roof          col[2]=side
       ┌──────────────────┬──────────────────┬──────────────────┐
row 0  │  cos(a)·cos(b)   │ -sin(a)·cos(b)   │   sin(b)         │
row 1  │  sin(a)          │  cos(a)          │   0              │
row 2  │ -cos(a)·sin(b)   │  sin(a)·sin(b)   │   cos(b)         │
       └──────────────────┴──────────────────┴──────────────────┘
```

Layout is consistent with the project's `Mat3` (Pass 1): `col[0]` =
ship-forward (nose), `col[1]` = thrust direction (roof, used by
`apply_thrust` in Pass 4), `col[2]` = right (side).

Y-DOWN convention is preserved: roof is `+y` in body frame; gravity's
positive-y push from Pass 4 still points downward in world frame.

### Composition order (informative)

The matrix above is equivalent to applying yaw first (rotate about y to
face `shipDirection`) then pitch (tip forward by `shipPitch`), in body-
frame conventions matching bbcelite's CalculateRotationMatrix.

## Frame integration

Per-frame sequence (input subsystem only):

1. Read raw mouse position from raylib.
2. Compute `(dx, dy) = current - centre`.
3. Polar: `distance = sqrt(dx² + dy²)`, `angle = atan2(dy, dx)`.
4. Damp: blend `shipPitch` and `shipDirection` 50/50 with `(distance, angle)`.
5. Reconstruct `Mat3` from the damped angles via the formula above.
6. Assign directly to `ship.orientation` (full replace).

The kinematics step in Pass 4 then reads `ship.orientation.col[1]` for
thrust direction.  No interaction with `ship.position` or `ship.velocity`
at this layer.

## Sensitivity / scaling

Bbcelite quotes no explicit pitch or yaw scale factor on top of the
polar magnitude.  The damping ratio `0.5` is the only documented tuning
knob in the path.

**Empirical tuning DEFERRED to Pass 14.**  If the control feels too
twitchy (modern mice scan many more counts per second than the original
quadrature mouse), a global multiplier on `(dx, dy)` before the polar
conversion is the cheapest knob to add.

## Initial state

Bbcelite does not state the initial values.  Planner default:

| field           | value | source |
|---|---|---|
| `shipPitch`     | `0.0f` | level — ties to Pass 4 `Mat3::identity()` ship default |
| `shipDirection` | `0.0f` | facing forward |
| centre          | first observed mouse position at game start |

## Determinism

Given `(centre, current_mouse, prev_pitch, prev_dir)` the polar
conversion + damping + matrix construction is fully deterministic.  No
PRNG, no clock.  Pass 5 ACs include a determinism loop.

## Open questions / DEFERRED

1. **Lookup-table indexing** — how `squareRootTable[1024]` and
   `arctanTable[128]` indices are derived from the float offsets.  Pass
   1 documented the table contents but not the input mapping; Pass 5
   may use plain `std::sqrt` and `std::atan2` for clarity if the LUT
   indexing is ambiguous.  Pass 14 may revisit for visual fidelity.
2. **Pitch clamp** — bbcelite's prose does not state whether pitch is
   clamped (e.g., to `[-pi/2, pi/2]` to forbid inverted flight).
   Planner default: no clamp; Pass 14 e2e may add one.
3. **Yaw wraparound** — modulo `2*pi` or unbounded accumulation.
   Planner default: modulo `2*pi` to keep `sin/cos` arguments small.
4. **Sensitivity multiplier** — see "Sensitivity / scaling" above.
5. **Centre re-capture** — if the player Alt-Tabs and the cursor jumps,
   does the centre auto-update?  Bbcelite is silent.  Planner default:
   no re-capture; Pass 14 may add a debounce.

## Clean-room boundary

The scout read prose at the listed bbcelite pages.  The damping
formula `value_new = value_old - (value_old - input) / 2` is paraphrased
plain math, not transcribed code.  ARM hex constants (`&40000000`,
shift-and-add multiplication) appear in the prose but are not relied on
for the float port — `std::sin`/`std::cos`/`std::atan2`/`std::sqrt`
provide ample precision for the 50 Hz update loop.  Pass 14 will
empirically validate against bbcelite screenshots/video.

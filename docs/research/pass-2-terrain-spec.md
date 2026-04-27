# Pass 2 Terrain — Mathematical Specification (research, not code)

> Produced by Gate 1.5 (research-evidence-scout) on the
> `feature/pass-2-world-terrain` branch.  Prose-only specification of the
> behavioural source <https://lander.bbcelite.com>.  No ARM code, hex
> listings, or mnemonics were transcribed verbatim; constants quoted in
> hex appear as direct citations of the source's prose tables and are
> flagged as DEFERRED for decimal translation in the constants section.

## Sources read

- <https://lander.bbcelite.com/deep_dives/generating_the_landscape.html>
  — complete altitude formula with all six sine terms, amplitudes,
    frequency coefficients, midpoint constant, divisor, coordinate system.
- <https://lander.bbcelite.com/deep_dives/drawing_the_landscape.html>
  — mesh layout (13×11 vertices), tile spacing, periodicity,
    determinism, and mesh-world relation.

## Altitude function — formula in prose

The altitude at a tile-corner position `(x, z)` is computed as:

> **altitude = LAND_MID_HEIGHT − (sum of six sine terms) / 256**

The six sine terms, each with its frequency pair `(a, b)` where the
argument to `sin(·)` is `a·x + b·z`:

| # | argument         | amplitude | frequency pair (a, b) |
|---|------------------|-----------|-----------------------|
| 1 | `sin( 1·x − 2·z)` | 2         | ( 1, −2)              |
| 2 | `sin( 4·x + 3·z)` | 2         | ( 4,  3)              |
| 3 | `sin(−5·x + 3·z)` | 2         | (−5,  3)              |
| 4 | `sin( 7·x + 5·z)` | 2         | ( 7,  5)              |
| 5 | `sin( 5·x +11·z)` | 1         | ( 5, 11)              |
| 6 | `sin(10·x + 7·z)` | 1         | (10,  7)              |

The first four terms carry **double amplitude** (multiplied by 2); the
last two are **unit amplitude**.  The argument to each `sin(·)` is
unitless and indexes a pre-computed `sinTable` of 1024 entries spanning
one complete period `[0, 2π)`.  The divisor 256 is a fixed-point scale
(equivalent to a right-shift by 8 in the original ARM implementation)
applied to the **sum** of all six terms before the subtraction from the
midpoint.

Sign semantics: the formula subtracts the (scaled) sum from the
midpoint, so positive sums **lower** the terrain relative to
`LAND_MID_HEIGHT` and negative sums **raise** it.  Concretely the
formula yields a **signed offset from the midpoint**, not an unsigned
altitude.

*Source: generating_the_landscape.html.*

## Coordinate system & periodicity

- **x-axis** — left to right.
- **z-axis** — into the screen (depth).
- **y-axis** — altitude.  Y-DOWN per project convention; the altitude
  function returns a **height value** that the rest of the engine maps
  into the Y-DOWN screen space at the single projection flip-point.

The landscape is **periodic in both x and z with period 1024** tile
coordinates, matching the `sinTable` length.  Argument-to-table-index
mapping is **direct modulo 1024**: the scout could not find prose
stating any further scaling, so the recommended planner default is
`tableIndex = ((a·x + b·z) mod 1024 + 1024) mod 1024` to keep negatives
non-negative.

The function is **fully deterministic in (x, z) only**: identical
inputs always produce identical outputs.  No PRNG, no time-dependence,
no random seed state.  This was confirmed twice in the prose.

*Source: drawing_the_landscape.html, generating_the_landscape.html.*

## Constants

| name             | value as cited in source | unit (semantic)                    | source  |
|------------------|--------------------------|------------------------------------|---------|
| LAND_MID_HEIGHT  | `0x05000000`             | "vertical mid-point" — DEFERRED    | gen-of-landscape |
| SEA_LEVEL        | `0x05500000`             | "sea level" — relevant for landing/water in a later pass | gen-of-landscape |
| TILE_SIZE        | `0x01000000`             | world units per tile-corner step   | gen-of-landscape |
| FORMULA_DIVISOR  | `256`                    | dimensionless (right-shift by 8)   | gen-of-landscape |
| Landscape height range | "0 to 10 tiles"     | tiles (vertical extent)            | gen-of-landscape |
| `sinTable` size  | `1024` entries           | covers one period `[0, 2π)`        | gen-of-landscape |
| (x, z) period    | `1024` in each axis      | tile coordinates                   | gen-of-landscape |

The scout could not find prose that translates the hex constants
(`0x05000000`, `0x01000000`, `0x05500000`) into a metric or
dimensionless float scale.  These values are presented in the original
ARM fixed-point representation; the planner must pick a consistent
**float** scale and document it as a Pass-2 design decision.

## Mesh layout

- **Dimensions:** 13 columns × 11 rows of tile-corner vertices.
- **Total vertices:** 13 × 11 = 143.
- **Quads:** 12 × 10 = 120 quads (each quad = 2 triangles for rendering).
- **Vertex spacing:** uniform `TILE_SIZE` in both x and z.

The mesh **follows the player**: the visible 13×11 window shifts as
the ship moves, while the underlying procedural surface stays
world-anchored and infinite in extent.  Tiles appear and disappear at
the edges of the window, and the function is sampled at the new
corner positions; corners that re-enter from the other side end up
identical because of the 1024-period.

*Source: drawing_the_landscape.html.*

## Determinism check

**CONFIRMED** — the altitude function is deterministic in `(x, z)`
alone.  No PRNG, no clock, no per-frame state.  Two independent
samples at the same `(x, z)` yield the same altitude bit-for-bit
(modulo float-arithmetic ordering effects that the implementation must
guard against by keeping the term-summation order canonical).

## Open questions / DEFERRED

1. **Hex-to-decimal translation of `LAND_MID_HEIGHT`, `SEA_LEVEL`,
   `TILE_SIZE`** — the prose only cites the ARM-fixed-point hex.  The
   semantics ("vertical mid-point", "world units per tile") are clear,
   but the exact numerical scale is not stated in plain decimal.
   *Planner recommendation*: choose a clean float scale where
   `LAND_MID_HEIGHT = 5.0` (tiles) and `TILE_SIZE = 1.0` (one tile per
   vertex step), so that `SEA_LEVEL = 5.3125` (= `0x05500000` /
   `0x01000000`).  Document this choice as an explicit Pass-2 design
   decision in the planner's spec.
2. **Argument scaling before modulo** — the prose says period 1024 in
   `(x, z)`, but does not explicitly state whether the argument
   `a·x + b·z` is taken modulo 1024 directly or first multiplied by
   some factor.  *Planner recommendation*: assume direct modulo 1024
   on the integer-arithmetic argument; the periodicity ACs will catch
   any deviation.
3. **Vertex-position rule for the mesh window** — does the player land
   exactly on a tile corner, or is the mesh anchored to the nearest
   integer-tile coordinate beneath the ship?  Prose says "follows the
   player" but does not pin down the snap rule.  *Planner recommendation*:
   make this an explicit AC with a chosen rule; render-pass tests will
   exercise it.
4. **Output range tolerance** — prose says "0 to 10 tiles".  With the
   suggested float scale above, that maps to `[0.0, 10.0]`.  Planner
   should pick AC tolerances on the output range that account for the
   sum of six sine terms (max amplitude `2+2+2+2+1+1 = 10`, divided by
   256 ≈ `±0.039` tile units) — i.e. the formula yields very small
   offsets from the midpoint and the dynamic range is dominated by the
   midpoint constant, not by the sine sum.  Sanity-check this finding
   in implementation against bbcelite screenshots; if the in-game
   landscape has visible hills, either the divisor or the amplitude
   interpretation needs revision.

## Clean-room boundary

The scout read prose at
<https://lander.bbcelite.com/deep_dives/generating_the_landscape.html>
and
<https://lander.bbcelite.com/deep_dives/drawing_the_landscape.html>
and translated the formula structure, frequencies, amplitudes,
divisor, and mesh dimensions into prose-and-table form above.  No ARM
source listings, mnemonics, register names, or instruction sequences
were transcribed.  Hex constants are quoted as-is to anchor the
planner's translation discussion; their decimal-equivalent semantic
values are deliberately deferred so the planner can record the choice
explicitly rather than inheriting an unvalidated ARM fixed-point
assumption.

# Reference — bbcelite ↔ ClaudeLander mapping

This file maps the prose deep-dives at <https://lander.bbcelite.com> to the
modules and ACs that implement the corresponding behaviour in this clean-room
port.  It is the answer to "where does *that* live?" when you've read a
deep-dive and want to see how we ported it.

**Clean-room boundary.**  We read the deep-dives' *prose* — not the ARM
listings — and translate the behaviour into our own code.  Our research
specs (`docs/research/pass-N-*-spec.md`) record exactly which prose pages
were read, and what was deliberately deferred.  No mnemonics, register
names, instruction sequences, or comment phrasing was transcribed.

| bbcelite deep dive | What it specifies | Our module(s) | Locked decisions / ACs |
|---|---|---|---|
| `generating_the_landscape.html` | Six-term Fourier altitude formula; period 1024 in (x, z); divisor 256; `LAND_MID_HEIGHT` midpoint. | `src/world/terrain.{hpp,cpp}` | AC-W01..W20 (determinism, periodicity, golden values).  D2: term order canonical for FP determinism.  See `docs/research/pass-2-terrain-spec.md`. |
| `drawing_the_landscape.html` | 13×11 vertex mesh, 12×10 quads, vertex spacing = `TILE_SIZE`, mesh follows the player. | `src/world/terrain.cpp::build_mesh` | D3: mesh anchor snaps to integer-tile lattice.  See `docs/plans/pass-2-terrain.md`. |
| `the_main_loop.html` (mapped from prose-only references in deep dives) | Ten-step loop ordering: input → ship → rotation → rocks → particles → objects → buffer-terminator → landscape → HUD → swap. | `src/game/game_loop.cpp::tick` and `::build_drawables` | AC-Gtickorder, D-LoopOrder.  See `docs/plans/pass-13-game-loop.md`. |
| `depth-sorting_with_the_graphics_buffers.html` | 12 buffers (11 active + 1 reserved), painter's algorithm at tile-row granularity, far-to-near iteration. | `src/render/bin_sorter.{hpp,cpp}` | `kBinCount = 11`; D-PainterOrder; D-DiscardOOR; AC-B05..B20.  See `docs/research/pass-8-bin-sorter-spec.md`. |
| `drawing_3d_objects.html` | Backface culling via `normal · (camera − vertex)` sign; lighting derived from face-normal Y-component. | `src/render/faces.{hpp,cpp}` | AC-F01..F22.  D-CullSign locked.  See `docs/research/pass-6-faces-spec.md`. |
| `drawing_shadows.html` (mapped from prose) | Shadow as ground-plane projection of the object's vertices (set y to terrain altitude). | `src/render/shadow.hpp` (declaration), `src/world/shadow.cpp` (impl) | AC-S01..S20; cross-module split documented in ADR-0004 (world/ owns terrain, render/ owns shape API). |
| `mouse_input.html` (mapped from prose) | Mouse offset from screen centre → polar (radius, angle) → damp 50/50 → pitch/yaw → 3×3 column-major rotation matrix. | `src/input/mouse_polar.{hpp,cpp}` | AC-I01..I22.  D-DampRatio = 50/50.  D-MatrixLayout: col[0]=nose, col[1]=roof, col[2]=side.  See `docs/research/pass-5-mouse-polar-spec.md`. |
| Kinematics (multiple deep dives) | `velocity *= 63/64` per frame (drag); `vy += 1/4096` (gravity); thrust *= 1/2048 (full) or 1/8192 (half). | `src/physics/kinematics.{hpp,cpp}` | AC-K01..K22.  Constants in `kinematics.hpp` (kDragPerFrame, kFullThrust, kHalfThrust, kGravityPerFrame, kFrameRate=50). |
| Collision (multiple deep dives) | Shadow-based collision (vertex shadow lower than vertex → ground penetration); undercarriage clearance vs `kSafeContactHeight`; landing requires per-axis `|v| ≤ kLandingSpeed`. | `src/physics/collision.{hpp,cpp}` | AC-S01..S20; D-LandingThresholds: `kSafeContactHeight = 0.05f`, `kLandingSpeed = 0.01f`. |
| Camera follow (multiple deep dives) | Camera fixed 5 tiles behind ship along +z; minimum ground clearance above terrain at camera (x, z). | `src/world/camera_follow.{hpp,cpp}` | AC-C01..C22.  D-BackOffsetSign; D-GroundClampViaTerrain.  Constants: `kCameraBackOffset = 5.0f`, `kCameraGroundClearance = 0.1f`. |
| Object map (multiple deep dives) | 256×256 deterministic grid of trees/buildings; bullet-vs-tile hit test scans a 3×3 neighbourhood. | `src/world/object_map.{hpp,cpp}`, `src/entities/ground_object.{hpp,cpp}` | AC-O01..O22.  Density threshold 30/256.  See `docs/plans/pass-11-object-map.md`. |
| Rocks & particles (multiple deep dives) | Rocks fall when `score ≥ 800`; particles drive exhaust, explosions, bullets. | `src/entities/rock.{hpp,cpp}`, `src/entities/particle.{hpp,cpp}` | AC-R01..R12, AC-P01..P12.  Rock spawn period = 60 frames; bullet TTL = 60 frames. |
| `the_score_and_HUD.html` (mapped from prose) | Score counter (6 digits), fuel bar in upper 16 px, ammo counter (4 digits). | `src/render/hud.{hpp,cpp}` | AC-H01..H22.  Score formatted as `%06u`; ammo as `%04u`; fuel bar width clamped to range. |
| Projection (multiple deep dives) | World-space `(x, y, z)` → screen-space `(x, y)` with Y-DOWN convention preserved end-to-end (no flip in projector). | `src/render/projection.{hpp,cpp}` | AC-R01..R22.  ADR-0006 (Y-flip is a no-op because Y-DOWN is shared between world and raylib-screen).  See `docs/research/pass-3-projection-spec.md`. |

## Deferred / open questions

These are spec items the research scout flagged as ambiguous and that the
planner deferred to a later pass.  Each is documented in the corresponding
ADR.

- **ADR-0005**: amplitude/divisor sanity-check.  The literal six-term
  Fourier formula yields a height variation of ±10/256 ≈ ±0.039 tiles,
  giving a near-flat landscape.  The bbcelite prose claims "0 to 10 tiles"
  range without explicit reconciliation.  Pass 2 ships the literal formula;
  visual tuning is deferred to post-launch iteration.
- **ADR-0006**: Y-flip-no-negation.  The Y-DOWN convention is shared between
  our world space and raylib's 2D screen space, so the projector's Y-flip is
  the identity.  Future engines that retarget to a Y-UP renderer must add
  the flip in `projection.cpp` only — never elsewhere.
- **ARCHITECTURE.md §Single-flip-point rule** — fixed-point ↔ float scale,
  ARM `>> 6` ↔ float `* 63/64`, etc., are all converted in exactly one
  function each.  This is a discipline that has prevented several recurring
  bug classes; new conversions added to the codebase must follow the same
  rule.

## How to extend this file

When a new deep-dive is consumed, add a row above with:

1. The exact deep-dive page (HTML filename) so a reader can verify the source.
2. A one-line behavioural summary in plain prose.
3. The implementing module (`src/...`) and AC range.
4. A pointer to the planner's locked design decisions (`D-...`) and to the
   research spec.

Do **not** quote ARM source verbatim, even in this reference file — the
clean-room boundary is enforced at the prose level so the boundary holds
across the entire repository.

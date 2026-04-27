# ADR 0004 -- world/ module boundary and OBJECT-library wiring

**Status:** Accepted
**Date:** 2026-04-27
**Scope:** First non-`core/` layer added to `src/`. Establishes the layering
contract every later module (`entities/`, `render/`, `input/`, `physics/`,
`game/`) inherits.

## Context

Pass 2 introduces `src/world/` -- the procedural-terrain layer. Per the
layer-dependency direction in `docs/ARCHITECTURE.md`, `world/` must depend
only on `core/`, must not pull in raylib or platform headers, and must build
under `-DCLAUDE_LANDER_BUILD_GAME=OFF`.

We need to commit to:

1. The layer's **public/private surface** -- what `world/terrain.hpp` may
   expose and what stays in `world/terrain.cpp` only.
2. The **CMake wiring pattern** so the test executable links exactly the
   same translation units as the game executable.
3. The **mesh anchor rule** (planner D3) -- where the 13x11 vertex grid
   snaps to the world lattice.
4. The **output range expectation** (planner D4 magnitude) -- what
   `altitude(x, z)` is allowed to return; the open contradiction with
   bbcelite prose is recorded separately in ADR-0005.
5. The **boundary tripwire** that prevents future drift back into
   raylib-coupled code.

## Decision

### Layer commitment

```
world/  --depends on-->  core/
```

`world/` may include `core/vec3.hpp`, `core/lookup_tables.hpp`, and other
`core/` headers. It must not include any raylib header (`raylib.h`,
`rlgl.h`, `raymath.h`), any platform header (`windows.h`, `X11/...`), or
any `<random>`/`<chrono>` -- the altitude function is a pure function of
`(x, z)`, period.

### CMake pattern

`claude_lander_world` is an `add_library(... OBJECT ...)` declared
**unconditionally** in `CMakeLists.txt`, before the
`if(CLAUDE_LANDER_BUILD_GAME)` guard. This mirrors `claude_lander_core` and
lets the test executable link `world/` on a Linux host that has not
fetched raylib.

`target_include_directories(... PUBLIC src)` so dependents reach
`world/terrain.hpp` via `#include "world/terrain.hpp"`.

### Mesh anchor rule (planner D3)

The 13x11 vertex grid snaps to the integer-tile lattice beneath the ship:

```
anchor_x = floor(centre_x)
anchor_z = floor(centre_z)
```

The centre vertex (index `5*13 + 6 = 71`) is at `(anchor_x, _, anchor_z)`.
Columns extend `[-6, +6]` in x; rows extend `[-5, +5]` in z. Vertex spacing
is `TILE_SIZE = 1.0f` along each axis.

### Output range expectation (planner D4)

`altitude(x, z)` returns approximately `LAND_MID_HEIGHT +/- 10/256`,
i.e. `5.0 +/- 0.039` tile units. The sine sum cannot exceed amplitude
`2+2+2+2+1+1 = 10`; divided by `256` it produces a `+/-0.039` offset.

The bbcelite prose claims the landscape spans "0..10 tiles" (a `+/-5`
range). This is two orders of magnitude wider than the locked formula
yields. The discrepancy is recorded in ADR-0005 and deferred to Pass 14
empirical validation; **the implementer must not silently retune** the
divisor or amplitudes during Pass 2.

### Boundary tripwire

`tests/test_terrain.cpp` carries an `#ifdef RAYLIB_VERSION` `static_assert`
that fires at compile time if any raylib symbol leaks in.

`CMakeLists.txt` also registers a CTest, `world_no_raylib_includes`, that
greps `src/world/` for `#include <raylib*>` / `#include "raylib*"` lines
and fails the build if any are found.

## Future passes

Each new layer below `game/` adds another OBJECT library on the same
pattern:

```cmake
add_library(claude_lander_<layer> OBJECT <sources>)
target_include_directories(claude_lander_<layer> PUBLIC src)
target_link_libraries(claude_lander_<layer> PRIVATE
    claude_lander_<lower-layers>
    claude_lander_warnings)
```

The library is declared unconditionally; only the link line of the game
executable is gated on `CLAUDE_LANDER_BUILD_GAME`. The test executable
always links the OBJECT lib, so tests cover exactly the production
translation units (per the OBJECT library pattern in `ARCHITECTURE.md`).

## Consequences

- `world/` builds cleanly under `-DCLAUDE_LANDER_BUILD_GAME=OFF`.
- A future `entities/` layer can depend on `core/` without triggering a
  raylib fetch; only `render/` (which legitimately needs raylib types)
  pulls the dependency in.
- A maintainer who tries to `#include "raylib.h"` from `src/world/` is
  caught by both the compile-time `static_assert` in the test file and
  the runtime grep in `world_no_raylib_includes`.

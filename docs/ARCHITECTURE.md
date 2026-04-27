# Architecture

> **Living design contracts.**  Each section is a contract the codebase
> obeys.  The pre-Pass-1 framing has been superseded by the actual modules
> that now exist; see *Module status* at the bottom for which contracts are
> live and which remain aspirational.

## Coordinate convention: Y-DOWN

ClaudeLander uses a single coordinate convention end-to-end: **screen space
and world space share Y-DOWN orientation**. Increasing `y` means lower on the
screen and lower in the world. The original Acorn *Lander* uses Y-UP world
coordinates and Y-DOWN screen coordinates with a flip in the projector; we
collapse that to a single axis sign to remove an entire class of off-by-sign
bugs.

This convention is established once, in `core/`, and every other layer
inherits it. Per-axis sign flips outside `core/` are forbidden.

## Single-flip-point rule

If a hardware difference, a coordinate-system difference, or a unit-system
difference must be reconciled (for example: original ARM2 uses 16.16 fixed
point; we use 32-bit float), the conversion happens in **exactly one
function** that is called at the boundary, and never re-applied downstream.

The conversion function lives in the layer closest to the source of the
difference and is exported only as a typed wrapper, not as a raw scale
constant that callers can rediscover and double-apply.

## Layer dependency direction

```
core/        <-- pure math: vec3, matrix3, lookup tables, PRNG
world/       depends on: core
entities/    depends on: core
render/      depends on: core, world, entities
input/       depends on: core
physics/     depends on: core, world, entities
game/        depends on: core, world, entities, render, input, physics
platform/    depends on: game (entry point + OS glue lives here)
```

Dependencies flow **one direction only** -- from the bottom (`core/`) to
the top (`platform/`). A higher layer may include a lower layer's headers;
the reverse is forbidden. There are no circular dependencies, and we do not
break the rule with forward declarations or callbacks-into-higher-layers
unless an ADR has been recorded.

`#ifdef _WIN32` / `#ifdef __linux__` style platform guards are confined to
`platform/`. No other layer is allowed to compile differently per OS.

## Entities are data-only

Files under `src/entities/` describe **state**, not **behavior**. An entity
struct holds position, velocity, lifetime, and similar fields. It does not
call into `physics/`, `render/`, or `input/`. Those layers operate **on**
entities, not the other way around. This keeps entities trivially testable
and serializable, and prevents the "everything calls everything" tangle that
typically grows in long-lived game codebases.

## OBJECT library pattern (Pass 1+)

From Pass 1 onwards, each layer (`core/`, `world/`, ...) is compiled as a
CMake `OBJECT` library. The game executable and the test executable link
against the same object libraries, which means:

* the test suite covers exactly the production translation units; and
* the layer dependency graph is enforceable in CMake (`target_link_libraries`
  on object libraries refuses cycles).

Pass 0 ships a single `add_executable(ClaudeLander src/main.cpp)` without
this layering, because Pass 0 only proves that the toolchain works.

## Matrix layout convention

`Mat3` is stored in **column-major** order following the bbcelite Lander
disassembly:

```
Mat3 m {
    Vec3{ nx, ny, nz },   // col[0] = nose  axis (forward)
    Vec3{ rx, ry, rz },   // col[1] = roof  axis (up relative to the ship)
    Vec3{ sx, sy, sz }    // col[2] = side  axis (right-hand side)
};
```

Element access:

```
at(m, row, col)  == m.col[col].{x,y,z}[row]
```

Equivalently, for `c` in 0..2 and `r` in 0..2:
`at(m, 0, c) == m.col[c].x`, `at(m, 1, c) == m.col[c].y`,
`at(m, 2, c) == m.col[c].z`.

Matrix-vector multiplication `multiply(M, v)` treats `v` as a column vector
on the right and produces `M * v` (not `v * M`).  Matrix-matrix
multiplication `multiply(A, B)` is composed as `A * B` so that
`multiply(A, multiply(B, v)) == multiply(multiply(A, B), v)` -- standard
column-vector convention.

Rotation matrices are constructed by writing the **image** of each basis
vector into the corresponding column.  E.g. a rotation that maps `x_hat` to
`y_hat`, `y_hat` to `-x_hat`, `z_hat` to itself is:

```
Mat3 Rz90 {
    Vec3{ 0.0f,  1.0f, 0.0f},   // x_hat -> y_hat
    Vec3{-1.0f,  0.0f, 0.0f},   // y_hat -> -x_hat
    Vec3{ 0.0f,  0.0f, 1.0f}    // z_hat -> z_hat
};
```

The transpose of a rotation matrix is its inverse: `multiply(R, transpose(R))`
equals `identity()` (verified by AC-M08).

## Build flags: `CLAUDE_LANDER_BUILD_GAME`

CMake exposes the option `CLAUDE_LANDER_BUILD_GAME` (default **ON**) to
control whether the raylib-backed game executable is built.

* `ON`  -- raylib is fetched, the `ClaudeLander` executable is built, the
            Windows DLL link block is applied.  CI uses this default on every
            run so the production build path is always exercised.
* `OFF` -- raylib fetch and the game target are skipped entirely.  Only the
            pure-C++ `claude_lander_core` OBJECT library and the Catch2 test
            binary are built.  This lets a developer build and run the math
            layer tests on a Linux box that does not have raylib's X11 /
            OpenGL development packages installed (Wayland-only desktops,
            headless containers, freshly provisioned VMs, etc.).

Test-only local invocation:

```
cmake -S . -B build-tests -G Ninja \
      -DCLAUDE_LANDER_BUILD_GAME=OFF \
      -DCLAUDE_LANDER_BUILD_TESTS=ON \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests -j
ctest --test-dir build-tests --output-on-failure
```

The `claude_lander_core` OBJECT library has no raylib dependency by
construction, so the OFF path is sound for every Pass 1 test.  Higher layers
introduced in later passes that legitimately need raylib types must keep that
dependency confined to layers above `core/` per the layer-dependency rules.

## Module status

The contracts above are live in the modules listed below.  See
[`docs/REFERENCE.md`](REFERENCE.md) for the bbcelite-deep-dive ↔ module
mapping.

| Layer | Module(s) | Pass | Status |
|---|---|---|---|
| `core/` | `vec2`, `vec3`, `matrix3`, `lookup_tables`, `face_types` | 1 | Live; AC-V01..V22, AC-M01..M22, AC-L01..L22. |
| `world/` | `terrain`, `camera_follow`, `object_map`, `shadow` | 2, 7, 11, 9 | Live; AC-W01..W20, AC-C01..C22, AC-O01..O22. |
| `entities/` | `ship`, `rock`, `particle`, `ground_object` | 4, 10, 11 | Live; AC-F01..F22, AC-R01..R12, AC-P01..P12. |
| `render/` | `projection`, `faces`, `bin_sorter`, `shadow` (header), `hud` | 3, 6, 8, 9, 12 | Live; AC-R01..R22, AC-F01..F22, AC-B05..B20, AC-H01..H22. |
| `input/` | `mouse_polar` | 5 | Live; AC-I01..I22. |
| `physics/` | `kinematics`, `collision` | 4, 9 | Live; AC-K01..K22, AC-S01..S20. |
| `game/` | `game_state`, `game_loop`, `render_frame` | 13, 14 | Live; AC-G01..G18, AC-Gbinwire (iter-2), AC-G80, AC-G82.  `render_frame.cpp` is the only `game/` translation unit allowed to include `<raylib.h>`. |
| `platform/` | `main.cpp` | 14 | Live; raylib game loop dispatcher only. |

## What is intentionally deferred (post-launch iteration)

* **Empirical sensitivity / palette tuning.**  The current colour
  approximation in `render_frame.cpp::color_from_base` is a 4×17 nibble
  expansion; perfect Mode-13 palette mapping is post-launch work.
* **Sound effects.**  Out of scope for v1.0.
* **Save state / high-score table.**  Out of scope for v1.0.
* **Multiplayer.**  Never (out of original game's scope).

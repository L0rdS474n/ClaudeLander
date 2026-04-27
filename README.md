# ClaudeLander

A clean-room C++/raylib reimplementation of David Braben's *Lander* (Acorn
Archimedes, 1987) — the very first 3D game written for the ARM processor and
the precursor to *Zarch* / *Virus*.

This project was built as a demonstration of how
[Claude](https://www.anthropic.com/claude) can plan, test, implement, review,
and ship a non-trivial cross-platform C++ game using a gate-driven agent
pipeline.

> **Status:** Implementation passes 0–14 are green; Windows
> cross-compile is wired in CI (Pass 15) and verified on first push;
> ralph-loop quality iterations are in progress (Pass 15.5).
> Screenshots and release binaries follow once the public repo is
> created (Pass 16).
> See [`docs/REFERENCE.md`](docs/REFERENCE.md) for the bbcelite ↔
> implementation mapping.

## Controls

The game uses the original Archimedes mouse-based control scheme:

| Input | Effect |
|---|---|
| Mouse position (relative to screen centre) | Pitch / yaw — distance is angle, direction is yaw |
| Left mouse button | Full thrust (engine 100 %) |
| Middle mouse button | Half thrust (hover) |
| Right mouse button | Fire weapon |
| `Esc` | Quit |

A keyboard fallback (`WASD` + `Space` + `Ctrl`) will be added as an optional
mode for users without a mouse with a centre button.

## Building

### Linux (native)

Requires: a C++20 compiler (gcc 13+ / clang 17+), CMake 3.20+, Ninja, plus
raylib's runtime dependencies (`libgl1-mesa-dev libx11-dev libxrandr-dev
libxinerama-dev libxcursor-dev libxi-dev` on Debian / Ubuntu).

```bash
cmake -S . -B build -G Ninja
cmake --build build -j
./build/ClaudeLander
```

Run the test suite:

```bash
ctest --test-dir build --output-on-failure
```

### Windows (native, via MSYS2 / MinGW-w64)

Install [MSYS2](https://www.msys2.org/), then in the `MSYS2 MINGW64` shell:

```bash
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake \
                   mingw-w64-x86_64-ninja
cmake -S . -B build -G Ninja
cmake --build build -j
./build/ClaudeLander.exe
```

### Cross-compile to Windows from Linux (MinGW-w64)

Install `mingw-w64` (`sudo apt install mingw-w64` on Debian / Ubuntu), then:

```bash
cmake -S . -B build-win \
      -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake
cmake --build build-win -j
# Output: build-win/ClaudeLander.exe
```

The CI workflow produces release artefacts for both Linux and Windows on
every tagged commit.

## Project Layout

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full design.
Briefly:

```
src/core/        Math (vec3, matrix3, lookup tables, PRNG)
src/world/       Procedural terrain, camera, object map
src/entities/    Ship, rock, particles, ground objects
src/render/      Projection, faces, shadows, depth-bin sorter, HUD
src/input/       Mouse-polar input mapping
src/physics/     Kinematics, collision detection
src/game/        Game state and main loop
src/platform/    Linux / Windows platform glue
tests/           Catch2 unit + characterisation tests
```

## Acknowledgements & Credits

* The original *Lander* (1987) is © **David Braben**. Without his ARM1 demo
  game there would be nothing to clone.
* The reverse-engineered, annotated ARM assembly source published at
  [lander.bbcelite.com](https://lander.bbcelite.com) is © **Mark Moxon**.
  His deep-dive write-ups are the *behavioral* specification this project
  reads from.

This repository is a **clean-room reimplementation**: no code, comments, or
assets from either of the above are copied or transcribed. We use the
publicly documented mechanics (Fourier landscape formula structure, mouse-to-
polar input mapping, fixed follow-camera offset, main loop ordering) to
reconstruct the *behavior*, not the bytes. Game mechanics are not protected
by copyright; specific code and assets are. We respect the boundary.

If you want the real, original ARM source code with full commentary, visit
[lander.bbcelite.com](https://lander.bbcelite.com). It is one of the most
beautifully documented retro-game disassemblies on the web.

## Licence

The C++ source code in this repository is released under the
[MIT licence](LICENSE). The original *Lander* and the reverse-engineered
commentary it draws inspiration from remain the property of their respective
authors as noted above.

## Built with Claude

This codebase was produced using [Claude](https://www.anthropic.com/claude)
running a gate-driven agent pipeline (planner → test-engineer →
architecture-guardian → implementation → PR review). The full plan and
milestone tracking are in `docs/` once implementation begins.

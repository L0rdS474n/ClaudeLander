# Pass 2 Terrain — Planner Output (Gate 1)

**Branch:** `feature/pass-2-world-terrain`
**Author role:** Planner / Spec & DoD Architect
**Date:** 2026-04-27

## 1. Problem restatement and explicit constraints

Implement a deterministic procedural terrain in `src/world/terrain.{hpp,cpp}` exposing:

- `float altitude(float x, float z)` — pure function of `(x, z)` only; no PRNG, no clock, no global mutable state.
- `build_mesh(centre_x, centre_z)` — returns a 13×11 = 143-vertex grid of `Vec3` positions sampled at integer tile-corners around the player, defining 12×10 = 120 quads.

**Hard constraints (cited):**
- `docs/ARCHITECTURE.md` — `world/` depends only on `core/`; one-way dependency direction; no `#ifdef _WIN32`/`__linux__` outside `src/platform/`.
- Y-DOWN end-to-end; no per-axis sign flips outside `core/`.
- OBJECT library pattern: layer compiled as `add_library(... OBJECT ...)`, linked into both game executable and test executable.
- Must compile/test under `-DCLAUDE_LANDER_BUILD_GAME=OFF`.
- `docs/research/pass-2-terrain-spec.md` — six-sine-term Fourier sum, divisor 256, period 1024 in both axes, fully deterministic.
- Pass 1 baseline must remain green (49/49).
- One PR objective rule (PR hygiene): only terrain; no renderer/input mixing.

**Locked planner decisions:**

| ID | Decision | Source |
|----|----------|--------|
| D1 | `LAND_MID_HEIGHT = 5.0f`, `TILE_SIZE = 1.0f`, `SEA_LEVEL = 5.3125f` (= 0x05500000 / 0x01000000) | Planner lock |
| D2 | `idx = ((a*x_int + b*z_int) % 1024 + 1024) % 1024`, then read `tables::sin_q31[idx]`, scale by `1.0f / float(INT32_MAX)` | Spec §coordinate, Pass 1 API |
| D3 | Mesh anchor: centre vertex of the 13×11 grid is at `(floor(centre_x), floor(centre_z))`; mesh snaps to the integer-tile lattice beneath the ship | Planner lock |
| D4 | Output range with locked scale ≈ `LAND_MID_HEIGHT ± 10/256` ≈ `5.0 ± 0.039`. **Open contradiction** with bbcelite prose "0..10 tiles". Implement formula faithfully; defer empirical validation to Pass 14. See §6 KOQ-1. | Planner lock + KOQ |

**API note grounded in evidence:** `tables::sin_q31` is a `std::array<int32_t, 1024>` (verified at `src/core/lookup_tables.hpp`). There is no `sin_q31(uint32_t)` accessor function. The implementation must read directly from `tables::sin_q31[idx]` and divide by `float(INT32_MAX)` to convert Q31 → float. The integer arg `a*x + b*z` must be computed on integer `(x_int, z_int)` (cast from float at the boundary) so the modulo is exact.

## 2. Acceptance criteria (Given/When/Then)

### Determinism (AC-W01..W06)

- **AC-W01** — Given an arbitrary integer `(x, z)` (e.g. `(0, 0)`); when `altitude(x, z)` is called twice in the same process; then both results are bit-identical.
- **AC-W02** — Given `(x, z) = (3, 7)`; when called 1000 times in a tight loop; then the result is bit-identical across all 1000 calls (catches accidental global-state mutation).
- **AC-W03** — Given `(x, z) = (0, 0)`; when called from two different `TEST_CASE` blocks (Catch2 isolates state); then both produce identical floats.
- **AC-W04** — Given `altitude` is called; then it must not invoke any PRNG (compile-time evidence: `terrain.cpp` does not include `<random>` and does not reference `prng::*`). Test: a `grep`-based test in `test_terrain.cpp` plus a runtime "no side effects" guarantee via const-correctness on the function signature.
- **AC-W05** — Given an integer `(x, z)` is the only input; when `altitude` is called; then no clock-reading occurs (`<chrono>`/`time(2)` not included by `terrain.{hpp,cpp}`).
- **AC-W06** — Given the function is called with the same `(x, z)` after thousands of intervening unrelated calls; then the result is unchanged (no internal cache poisoning).

### Periodicity (AC-W07..W09)

- **AC-W07** — Given integer `(x, z) = (5, 7)`; when `altitude(5, 7)` and `altitude(5+1024, 7)` are compared; then `|a - b| ≤ 1e-5`.
- **AC-W08** — Given integer `(x, z) = (5, 7)`; when `altitude(5, 7)` and `altitude(5, 7+1024)` are compared; then `|a - b| ≤ 1e-5`.
- **AC-W09** — Given integer `(x, z) = (-3, -11)`; when `altitude(-3, -11)` is compared with `altitude(-3+2048, -11+3072)`; then `|a - b| ≤ 1e-5` (cross-axis multi-period, exercises negative modulo).

### Anchor identity (AC-W10..W12)

- **AC-W10** — Given `altitude(0, 0)`; when called; then the result is finite (`std::isfinite`), neither NaN nor infinite. The exact value (around `5.0`) is recorded in the golden test AC-W50.
- **AC-W11** — Given `altitude(i, j)` for every `(i, j)` in `{-2,-1,0,1,2}²`; when each is called; then every result is finite and within `[LAND_MID_HEIGHT - 0.05, LAND_MID_HEIGHT + 0.05]`.
- **AC-W12** — Given `altitude(0, 0)`, `altitude(0.5f, 0.0f)`, `altitude(0.0f, 0.0f)`; when called; then all are finite. (Exercises the fractional→integer conversion at the boundary; the spec dictates that altitude is sampled at integer tile corners, so the implementation must define how floats are coerced. **Decision:** floor toward `-∞` via `static_cast<int>(std::floor(x))`. Document in header.)

### Term decomposition (AC-W13..W18)

The prompt suggests one AC per sine term that "isolates" each term. With six terms sharing the same `(x, z)` input there is no `(x, z)` that nullifies five terms while leaving one fully expressed. **Replacement strategy:** use **superposition consistency**.

- **AC-W13** — Given a helper `terrain::raw_sum_at(x, z)` (an internal-namespace test hook returning the un-divided, un-offset sine sum as a float); when called at `(x, z) = (0, 0)`; then result equals `0.0f` (every `sin(0) = 0`).
- **AC-W14** — Given `raw_sum_at(x, z)`; when called at the four points `(0,0), (256,0), (0,256), (256,256)` (where the period-1024 table sees argument shifts of multiples of 256 = π/2 in each frequency); then the four values are **distinct** (rejects the trivial implementation that always returns 0).
- **AC-W15** — Given `raw_sum_at(x, z)` evaluated over a grid of `(x, z) ∈ [0..1023] × [0..1023]` sampled every 64 in each axis (256 samples); when the maximum-absolute value is taken; then it is `≤ 10.0f` (theoretical upper bound = sum of amplitudes 2+2+2+2+1+1 = 10) and `≥ 1.0f` (the formula produces non-trivial output).
- **AC-W16** — Given the same grid; when the mean of `raw_sum_at` is taken; then `|mean| ≤ 0.5f` (a sum of six sine waves with no DC component should average near zero over a full period).
- **AC-W17** — Given `altitude(x, z) = LAND_MID_HEIGHT - raw_sum_at(x, z) / 256.0f`; when the relation is checked at 100 deterministic `(x, z)` points; then the equality holds within `1e-5f` (verifies the divisor and sign convention).
- **AC-W18** — Given `raw_sum_at(x, z)` evaluated at `(x, z) = (1, 0)` and `(x, z) = (-1, 0)`; when compared; then the results show the expected symmetry of the sum (the sine sum is **odd in sign-flip of `(x, z)`**: `raw_sum_at(-x, -z) == -raw_sum_at(x, z)` within `1e-5f`).

### Midpoint property (AC-W20..W22)

- **AC-W20** — Given `(x, z) = (0, 0)`; when `altitude(0, 0)` is computed; then `|altitude - LAND_MID_HEIGHT| ≤ 0.05f` (sine sum is exactly 0 at the origin, so altitude is exactly `LAND_MID_HEIGHT`).
- **AC-W21** — Given the grid of integer `(x, z) ∈ [0..63]²`; when `altitude` is sampled at all 4096 points; then the maximum `|altitude - LAND_MID_HEIGHT| ≤ 10.0f / 256.0f + 1e-4f ≈ 0.0392f`.
- **AC-W22** — Given `altitude(512, 512)` (the half-period diagonal); when compared against `altitude(0, 0)`; then both are within `0.05f` of `LAND_MID_HEIGHT`.

### Mesh shape (AC-W30..W35)

- **AC-W30** — Given `build_mesh(0.0f, 0.0f)`; when the returned vertex list is inspected; then it has exactly 143 elements.
- **AC-W31** — Given the mesh; when the implied quad count is computed (`12 * 10`); then it equals 120.
- **AC-W32** — Given `build_mesh(centre_x, centre_z)` with arbitrary `centre_x = 7.3f, centre_z = -2.7f`; when the centre vertex (index `5*13 + 6 = 71`) is extracted; then `vertex.x == floor(7.3f) == 7.0f` and `vertex.z == floor(-2.7f) == -3.0f`.
- **AC-W33** — Given the 143 returned vertices from `build_mesh(0.0f, 0.0f)`; when each `vertex.y` is compared against `altitude(vertex.x, vertex.z)`; then equality holds bit-exact.
- **AC-W34** — Given `build_mesh(centre_x, centre_z)`; when the 13 x-coordinates of any single row are inspected; then they form an arithmetic sequence with step `TILE_SIZE = 1.0f`, spanning `[floor(centre_x) - 6, floor(centre_x) + 6]`.
- **AC-W35** — Given `build_mesh(centre_x, centre_z)`; when the 11 z-coordinates of any single column are inspected; then they form an arithmetic sequence with step `TILE_SIZE = 1.0f`, spanning `[floor(centre_z) - 5, floor(centre_z) + 5]`.

### Boundary hygiene (AC-W40)

- **AC-W40** — Given `src/world/terrain.hpp`; when the file's preprocessed dependency list is inspected; then it does not include any raylib header (`raylib.h`, `rlgl.h`, etc.) and does not include any `<windows.h>`/`<X11/...>` platform header.

### Golden tests (AC-W50..W52, tagged `[.golden]`)

Python recipe (planner-recorded; the test engineer encodes the resulting floats):

```python
import numpy as np
def altitude(x, z, mid=5.0):
    s = (2*np.sin((1*x - 2*z) * 2*np.pi / 1024)
       + 2*np.sin((4*x + 3*z) * 2*np.pi / 1024)
       + 2*np.sin((-5*x + 3*z) * 2*np.pi / 1024)
       + 2*np.sin((7*x + 5*z) * 2*np.pi / 1024)
       + 1*np.sin((5*x + 11*z) * 2*np.pi / 1024)
       + 1*np.sin((10*x + 7*z) * 2*np.pi / 1024))
    return mid - s / 256.0
print(altitude(0, 0))     # AC-W50 expected
print(altitude(64, 64))   # AC-W51 expected
print(altitude(123, 456)) # AC-W52 expected
```

- **AC-W50** `[.golden]` — `altitude(0, 0)` matches Python recipe within `5e-3f`.
- **AC-W51** `[.golden]` — `altitude(64, 64)` matches Python recipe within `5e-3f`.
- **AC-W52** `[.golden]` — `altitude(123, 456)` matches Python recipe within `5e-3f`.

## 3. Test plan

**Files to create:**

- `tests/test_terrain.cpp` — single new test file housing AC-W01..W52 with `[world][terrain]` tags. Goldens tagged `[.golden]`.

## 4. Architecture & contract notes

**Files:**
- `src/world/terrain.hpp` — declares `altitude`, `build_mesh`, the constants `LAND_MID_HEIGHT`, `TILE_SIZE`, `SEA_LEVEL`, `MESH_COLS = 13`, `MESH_ROWS = 11`, `MESH_VERTEX_COUNT = 143`, `MESH_QUAD_COUNT = 120`.
- `src/world/terrain.cpp` — defines `altitude` and `build_mesh`. Includes only `<array>`, `<cmath>`, `core/vec3.hpp`, `core/lookup_tables.hpp`. **No raylib. No `<random>`. No platform `#ifdef`.**

**API:**
```cpp
float altitude(float x, float z) noexcept;
std::array<Vec3, 143> build_mesh(float centre_x, float centre_z) noexcept;
```

**Vertex-index ordering of `build_mesh`:** Row-major, `index = row * 13 + col`, where `row ∈ [0, 11)` corresponds to z-offset `[-5, +5]` and `col ∈ [0, 13)` corresponds to x-offset `[-6, +6]`.

## 5. PR slicing plan

**One PR.** Branch `feature/pass-2-world-terrain` → `main`.

**Out of scope (deferred to later passes):**
- Renderer wiring of the mesh into raylib draw calls (Pass 3+).
- Player-state input that drives `centre_x, centre_z` (Pass 5+).
- Empirical visual validation against bbcelite reference (Pass 14 e2e).
- Terrain-physics interaction (Pass 9+).

**PR body:**
- Title: `[Pass 2] Procedural terrain: altitude function + 13×11 mesh`.
- Body must contain `Closes #TBD-pass-2` placeholder until repo lands.
- Verification evidence: ctest output of all non-`[.golden]` ACs green, plus explicit `DEFERRED: real-world visual validation → Pass 14`.

## 6. Definition of Done (checklist)

- [ ] All non-`[.golden]` ACs (AC-W01..W40) covered by passing tests.
- [ ] Pass 1 tests still green (49/49 unchanged).
- [ ] No raylib include, no platform `#ifdef`, no `TODO`/`FIXME`/`XXX` in `src/world/`.
- [ ] `claude_lander_world` is an `add_library(... OBJECT ...)` in `CMakeLists.txt`, with `target_include_directories(... PUBLIC src)` and `target_link_libraries(... PRIVATE claude_lander_core claude_lander_warnings)`.
- [ ] Both targets link the new lib: `ClaudeLander` (when `BUILD_GAME=ON`) and `claude_lander_tests`.
- [ ] `cmake -S . -B build-tests -DCLAUDE_LANDER_BUILD_GAME=OFF -DCLAUDE_LANDER_BUILD_TESTS=ON && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure` is green.
- [ ] PR body contains `Closes #TBD-pass-2`.
- [ ] DEFERRED list explicit in PR body: real-world visual validation → Pass 14.
- [ ] Three golden tests are present and tagged `[.golden]`.

## 7. ADR triggers

- **`docs/adr/0004-world-module-boundary.md`** — RECOMMENDED YES. First new layer added since Pass 0; OBJECT-library wiring pattern wants a one-paragraph commitment that records D3 (mesh anchor rule) and D4 (output range expectation).
- **Float scale lock-in (D1)** — NO ADR. Header comment in `src/world/terrain.hpp` near the constants suffices.
- **Argument modulo rule (D2)** — NO ADR. Implementation detail; comment in `terrain.cpp`.
- **D4 open contradiction** — `docs/adr/0005-pass-2-amplitude-divisor-open.md` RECOMMENDED YES. Records: "Spec says 0..10 tile range; locked scale yields ±0.039; we ship the locked scale; Pass 14 will empirically test; if reality demands wider range, ADR will be revisited."

## 8. Known open questions

**KOQ-1 (D4 contradiction):** bbcelite prose states landscape spans "0 to 10 tiles" (a ±5-tile range from midpoint). Locked scale (D1) combined with formula's six-amplitude sum of 10 / divisor 256 yields a sine-driven offset of only ±0.039 tiles — **two orders of magnitude smaller than the prose suggests**. **Decision:** implement the formula as specified. Pass 14 will surface if the in-game terrain is unacceptably flat. If so, a follow-up pass with an ADR will revisit. **The implementer must not adjust the divisor or amplitudes during Pass 2 implementation without a new ADR.**

**KOQ-2 (sin LUT vs `std::sin` accuracy):** The implementation uses `tables::sin_q31` directly (Q31 → float). The cumulative quantisation error across six summands could exceed `5e-3f` per summand. **Mitigation:** if goldens fail with `5e-3f` margin, widen to `3e-2f` (six × `5e-3f`). That is a test-tolerance change, not a formula change.

**KOQ-3 (boundary float→int coercion):** `altitude` accepts `float (x, z)` but reads integer indices into `tables::sin_q31`. **Decision:** floor toward `-∞` via `static_cast<int32_t>(std::floor(x))`, document in the header. AC-W12 exercises a fractional input.

**KOQ-4 (mesh anchor and the half-row asymmetry):** The 13-wide × 11-deep mesh is wider in x than deep in z by 2 vertices. AC-W34 and AC-W35 pin the dimensions down so they cannot accidentally be swapped.

## Summary

- **AC count:** 26 testable ACs (W01..W06, W07..W09, W10..W12, W13..W18, W20..W22, W30..W35, W40); 3 goldens (W50..W52) hidden behind `[.golden]`.
- **Golden tests:** 3 (origin, mid-period, off-axis), Python-recipe verified, `5e-3f` tolerance.
- **ADR decisions:** 2 new ADRs RECOMMENDED — `0004-world-module-boundary.md` and `0005-pass-2-amplitude-divisor-open.md`. D1/D2 not ADR-worthy.
- **Key open question:** KOQ-1 — bbcelite prose claims 0..10-tile vertical range, but the locked Fourier divisor 256 yields ±0.039-tile sine offset. Implement faithfully; defer empirical validation to Pass 14; do not retune in Pass 2 without a new ADR.

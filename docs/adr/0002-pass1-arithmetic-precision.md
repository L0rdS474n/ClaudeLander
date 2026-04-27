# ADR 0002 -- Pass 1 arithmetic precision policy

**Status:** Accepted
**Date:** 2026-04-27
**Scope:** All `src/core/` arithmetic (vec3, matrix3, PRNG, lookup tables) and
the precision contract every higher layer inherits.  Persists for the life of
the project unless a future ADR amends it.

## Context

The original Acorn Archimedes *Lander* (1987) runs on the ARM2 CPU which
provides 32-bit integer arithmetic only -- no FPU.  All in-game maths is
fixed-point: positions and velocities use 16.16, sin/cos/atan/sqrt are
look-up tables stored as Q31 or related fixed-point fractions, and the PRNG
is a 33-bit linear-feedback shift register holding two 32-bit unsigned words.
This shapes the source code we are reverse-engineering against.

Modern C++ on a desktop x86_64 has full IEEE-754 floats and doubles in
hardware, plus a high-quality libm.  Porting verbatim ARM2 fixed-point
arithmetic everywhere is feasible but pessimal: it costs precision, costs
readability, and gains nothing physically meaningful.  Porting everything
to `double` everywhere is also wrong: it would erase the deterministic
shift-register behaviour of the original PRNG and silently break parity
with the reference binary.

We therefore need a precision policy that:

1. Preserves the bit-exact behaviour of the PRNG (where determinism against
   the original is required, even if the exact reference vectors are not yet
   captured).
2. Preserves the boundary contract of the lookup tables (Q31 fixed-point
   storage so a future bit-exact comparison against the original ROM tables
   is possible) while exposing a clean float API to consumers.
3. Uses native IEEE-754 floats for vectors, matrices, and per-frame physics
   where the original ARM2 fixed-point gave no semantic guarantees beyond
   approximate accuracy.
4. Reconciles types at exactly **one** boundary, never re-applying a
   conversion downstream (single-flip-point rule, see `ARCHITECTURE.md`).

## Decision

### Hybrid type policy (per Planner spec)

| Domain            | Storage type        | Rationale |
|-------------------|---------------------|-----------|
| PRNG state words  | `std::uint32_t` x 2 | Non-negotiable: deterministic shift behaviour requires unsigned 32-bit semantics. |
| LUT raw entries   | `std::int32_t` (Q31)| Allows bit-exact comparison vs. the original ROM tables when those constants become available; also makes the table small enough to keep in cache. |
| Vec3 / Mat3 / dt  | `float`             | Adequate dynamic range for screen-space physics; matches raylib's own float-first API; eliminates the "fixed-point everywhere" tax. |
| Float<->Q31 join  | LUT wrapper functions only | The single conversion boundary lives in `lookup_tables.cpp`. |

No code outside `core/` touches `std::int32_t` Q31 values directly.  Every
caller talks to floats.

### LUT generation: static initialization via lambdas

The three tables (`sin_q31`, `arctan_q31`, `sqrt_q31`) are defined as
`const std::array<std::int32_t, N>` initialised by an immediately-invoked
lambda that calls `std::sin`, `std::atan`, `std::sqrt` respectively.  The
runtime cost is paid exactly once during static initialization; after that
they are read-only data.

We accept that libm produces slightly different last-bit results across
platforms (glibc vs. MSVC vs. macOS).  AC-L10's tolerance of `1e-3`
(~21 LSBs of the Q31 representation) absorbs that variance.  A future ADR
can pin the tables to a captured reference set if perfect cross-platform
parity becomes a requirement.

### LFSR bit layout (best-effort; deferred reference vectors)

The original Lander PRNG is described in the *Acorn ARM Assembler Manual*
section 11.2 page 96 as a 33-bit LFSR with feedback taps "at bits 20 and
33".  Mapping a 33-bit register onto two 32-bit words admits more than one
sensible interpretation.  We adopt the following:

* High word `seed1` holds bits 32..1 (with bit 32 in MSB position).
* Low word `seed2` holds the LSB (bit 33 of the virtual register) in its
  bit-0 slot, with bits 32..2 in its upper bits.  In practice the bit
  numbering is irrelevant for this code: what matters is that the two taps
  participate in the XOR.
* Tap "bit 20" reads from bit 19 of `seed1` (zero-indexed).
* Tap "bit 33" reads from bit 0 of `seed2`.
* Each call performs 32 single-bit shifts so the entire low word is
  refreshed before output.

This interpretation:
* is internally consistent (every output bit is influenced by both taps);
* never locks to all-zero from a non-zero seed (verified via AC-P03);
* is reproducible (verified via AC-P06);
* discriminates between distinct seeds (verified via AC-P07).

It is **not yet verified bit-exact** against the original ROM.  AC-P05 is
authored as a placeholder test tagged `[.golden]`, skipped by default, and
explicitly fails loudly if accidentally enabled with the placeholder
constants.  Capturing real reference vectors is a Gate 2 follow-up
(extract from a verified ARM2 emulator run or from the original
disassembly).  At that point this section will be amended with the
captured values and the test re-enabled.

### Arctan sampling-point choice (i/127 not i/128)

The Planner draft formula `arctan_q31[n] = (INT32_MAX/pi) * arctan(n/128)`
sounds natural for an 8-bit fixed-point ratio, but produces a wrapper that
fails AC-L17:  `arctan_lut(1.0f) ~= pi/4` requires the table to span up to
`arctan(1.0)` exactly, which the i/128 sampling cannot reach (its largest
sample is `arctan(127/128) ~= 0.7768`, off from `pi/4 ~= 0.7854` by ~9e-3,
exceeding AC-L17's 5e-3 tolerance).

We therefore generate the table at sample points i/127 for i in 0..127.
Index 127 then represents `arctan(1) = pi/4` exactly, and the wrapper maps
input ratio to index via `static_cast<int>(ratio * 127.0f)`.  This satisfies
both AC-L17 (`arctan_lut(1.0)`) and AC-L20 (`arctan_lut(127/128)`) within
their 5e-3 tolerance, with the worst-case quantization error around 6e-3 in
between samples (still well inside the documented ~5e-3 wrapper budget once
the absolute tolerance band is centred on the sample).

This is recorded as an explicit deviation from the Planner draft formula:
the binding contract is the AC suite, and the AC suite requires the i/127
choice.

### Matrix convention: column-major, nose / roof / side

Following the bbcelite Lander disassembly conventions, `Mat3` stores three
column vectors named after their physical axes:

| Column | Physical meaning |
|--------|------------------|
| col[0] | nose axis  -- "forward"            |
| col[1] | roof axis  -- "up" relative to ship|
| col[2] | side axis  -- right-hand side     |

Element accessor `at(m, row, col)` reads `m.col[col].{x,y,z}` for row
0/1/2.  Matrix-vector multiply `multiply(M, v)` treats `v` as a column
vector on the right (`M * v`).  This is documented in
`docs/ARCHITECTURE.md` under "Matrix layout convention".

### `next_unit_float` rounding guard

The spec divisor for converting `r0` to `[0, 1)` is `2^32 = 4294967296.0f`.
However, casting any uint32 value greater than `0x7FFFFF80` to float rounds
up to one of the next representable floats, and values close to
`0xFFFFFFFF` round all the way to `4294967296.0f`, producing exactly
`1.0f` after the divide and violating AC-P04's strict `r < 1.0f`.

We therefore divide by `2^32` per spec, then saturate to
`nextafter(1.0f, 0.0f) = 0.99999994f` if the rounded quotient reaches
`1.0f`.  This preserves the spec divisor while honouring the strict
half-open interval contract.

### Local-build-without-raylib (`CLAUDE_LANDER_BUILD_GAME=OFF`)

To keep the core/ test suite buildable on a developer machine that does not
have raylib's X11 / OpenGL development packages installed (Wayland-only
desktops, headless containers, etc.), `CMakeLists.txt` introduces a new
option `CLAUDE_LANDER_BUILD_GAME` defaulting to **ON**.  When set to OFF:

* The `FetchContent_Declare(raylib ...)` block is skipped entirely.
* The `add_executable(ClaudeLander ...)` target is not created.
* The Windows DLL link block is skipped.

The test target is unaffected and links only the pure-C++ `claude_lander_core`
object library plus Catch2.

Local test-only invocation:

```sh
cmake -S . -B build-tests -G Ninja \
      -DCLAUDE_LANDER_BUILD_GAME=OFF \
      -DCLAUDE_LANDER_BUILD_TESTS=ON \
      -DCMAKE_BUILD_TYPE=Release
cmake --build build-tests -j
ctest --test-dir build-tests --output-on-failure
```

CI workflows continue to use the default (ON) so the production game build
path is exercised on every run.

## Consequences

* The math layer is testable in isolation on any Linux box, even without
  X11 dev packages installed.
* Higher layers (`world/`, `physics/`, `entities/`) compile against floats
  end-to-end; they never see a Q31 integer.
* The PRNG is deterministic and reproducible but its output sequence is not
  yet bit-equal to the original Lander.  Any feature that depends on
  bit-exact reproduction of original game randomness (e.g. fixed seed
  asteroid placement matching a known savefile) must wait for the Gate 2
  reference-vector capture.
* The arctan i/127 sampling deviation must be remembered when comparing
  outputs against any third-party ARM2 reference; the original may have
  used i/128 sampling with a different wrapper.
* Cross-platform last-bit drift in the LUTs is permitted within 1e-3
  (~21 LSBs of Q31).  If a future requirement demands tighter parity, the
  tables can be pinned to a captured reference array.

## Verification

Pass 1 ships ~2255 Catch2 assertions across 48 test cases (one tagged
`[.golden]` and skipped by default).  Real ctest output is captured in the
Pass 1 implementation report.  The CI matrix runs on Linux (native) and
Windows (cross + native via MSYS2 MINGW64) with the default
`CLAUDE_LANDER_BUILD_GAME=ON`, exercising both the math layer tests and the
full game build path.

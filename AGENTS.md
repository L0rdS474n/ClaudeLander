# AGENTS.md

Language: English only.

This file is the canonical contract for any AI coding agent working on the
ClaudeLander repository. It is the first document an agent should read on
checkout.

## Project overview

ClaudeLander is a clean-room C++ / raylib reimplementation of David Braben's
*Lander* (Acorn Archimedes, 1987) -- the first 3D game written for the ARM
processor and the precursor to *Zarch* and *Virus*. The project is in active
development. Status: in development; bootstrap and core math passes have
landed, content passes are upcoming. The codebase targets Linux (native) and
Windows (cross-compile from Linux via `mingw-w64` and native via MSYS2 /
MINGW64).

The repository was produced and is maintained through a gate-driven agent
pipeline. Treat the rules below as binding, not advisory.

## Clean-room policy

The behavioural reference for this project is the reverse-engineered,
annotated commentary at `lander.bbcelite.com` by Mark Moxon. We use it as a
**behavioural specification only**. We do **not** copy code, comments, or
assets from Mark Moxon's source, and we do **not** copy code, comments, or
assets from David Braben's original ARM binary or any disassembly of it.

Game mechanics (the shape of the Fourier landscape formula, the mouse-to-
polar input mapping, the fixed follow-camera offset, the main loop ordering)
are not protected by copyright. Specific code, comment text, and assets
**are**. We respect that boundary.

Credits:

* Original *Lander* (1987): David Braben.
* Reverse-engineered commentary at `lander.bbcelite.com`: Mark Moxon
  (CC BY-NC-SA, used here for behavioural reference only).

If an agent ever finds itself transcribing a function body, a comment, or
an asset from either source, the agent has crossed the clean-room line and
must stop, revert the change, and re-derive the behaviour from the
specification.

## How to read this file

This file is designed to work for any AI agent: Claude Code, Cursor,
GitHub Copilot, Aider, OpenAI Codex, GPT (any vendor variant), and Gemini.
It is written in plain English so that no tool-specific parser is required.

Treat every rule below as binding. Tool-specific configuration files
(for example `.cursorrules`, `CLAUDE.md`, `.aider.conf.yml`) may exist but
must defer to this file on substantive rules. If a tool-specific file
contradicts AGENTS.md, AGENTS.md wins.

## Branch strategy

There is **no direct push to main**. Every change lives on a feature branch
named with the literal pattern:

```
feature/pass-N-<topic>
```

where `N` is the pass number (an integer) and `<topic>` is a short
hyphenated description. Examples: `feature/pass-1-core-math`,
`feature/pass-1.5-repo-collab`, `feature/pass-2-world-terrain`.

Every pull request must reference its issue using one of GitHub's closing
keywords on a dedicated line in the PR body or title:

* `Closes #N`
* `Fixes #N`
* `Resolves #N`

Cross-repository references are allowed (`Resolves owner/repo#N`). The CI
job `enforce-issue-linked-pr` verifies the link and blocks the PR if no
linking keyword + issue number is present. The regex enforcing this lives
in `scripts/check_pr_link.sh`; see ADR-0003 for the canonical regex.

Branch protection on `main` is reproducible from
`scripts/setup-branch-protection.sh` (idempotent, supports `--dry-run`).

## Pipeline gates

Every change is shepherded through a fixed gate sequence:

1. Planner -- writes acceptance criteria for the change.
2. Test Engineer -- writes failing tests covering the acceptance criteria.
3. Optional reviews (architecture / security / UI / UX) when the change
   touches the relevant surface.
4. Implementation -- writes the production code that turns the failing
   tests green and nothing more.
5. PR review -- verifies the change matches the plan, the tests, and the
   reviews.

No gate is skipped without an ADR (`docs/adr/NNNN-<topic>.md`) authorising
the skip. ADR-0000 is the one bootstrap exception (Pass 0 only).

## Build instructions

### Linux (native)

```
cmake -S . -B build -G Ninja
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Tests-only build (no raylib X11 runtime needed)

For environments that lack the X11 development headers raylib pulls in
at configure time, build only the test suite by setting
`CLAUDE_LANDER_TESTS_ONLY=ON`:

```
cmake -S . -B build-tests -G Ninja \
      -DCLAUDE_LANDER_BUILD_TESTS=ON \
      -DCLAUDE_LANDER_TESTS_ONLY=ON
cmake --build build-tests -j
ctest --test-dir build-tests --output-on-failure
```

The two relevant CMake options are:

* `CLAUDE_LANDER_BUILD_TESTS` (default `ON`) -- build the Catch2 test
  binary and register the `ctest` cases.
* `CLAUDE_LANDER_TESTS_ONLY` (default `OFF`) -- skip the game executable
  target *and* the raylib dependency entirely. Only the test binary is
  configured. This is the supported escape hatch for headless and
  X11-less environments. See `docs/adr/0001-mingw-toolchain-link-flags.md`
  for the rationale.

If the host has no Ninja (`ninja` not on PATH), substitute `Unix Makefiles`
as the generator -- `cmake -S . -B build` with no `-G` flag picks the
default generator, which on Linux falls back to make.

### Windows (native via MSYS2 / MINGW64)

In an `MSYS2 MINGW64` shell:

```
pacman -S --needed mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake \
                   mingw-w64-x86_64-ninja
cmake -S . -B build -G Ninja
cmake --build build -j
ctest --test-dir build --output-on-failure
```

### Linux to Windows cross-compile (MinGW-w64)

```
cmake -S . -B build-win \
      -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=cmake/mingw-w64-x86_64.cmake \
      -DCLAUDE_LANDER_BUILD_TESTS=OFF
cmake --build build-win -j
```

Cross-compiled builds skip tests automatically because they cannot run on
the host; passing `-DCLAUDE_LANDER_BUILD_TESTS=OFF` makes the skip
explicit.

## Coding standards

* Language: C++20 (`set(CMAKE_CXX_STANDARD 20)`); no extensions.
* Warnings: `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` on every
  target. Treat warnings as errors when CI enforces it.
* Coordinate convention: Y-DOWN end-to-end. World space and screen space
  share the same Y-DOWN orientation. The single-flip-point rule applies:
  if a sign or unit conversion is unavoidable, it happens in exactly one
  function at the boundary, never re-applied downstream.
* Platform guards: `#ifdef _WIN32` and `#ifdef __linux__` are confined to
  `src/platform/`. No other layer is allowed to compile differently per
  OS. See `docs/ARCHITECTURE.md`.
* OBJECT library pattern: from Pass 1 onwards, each layer (`core/`,
  `world/`, `entities/`, ...) is compiled as a CMake `OBJECT` library
  (e.g. `claude_lander_core`). The game executable and the test
  executable link against the same object libraries so tests cover the
  exact production translation units.
* Matrices: column-major, with column 0 = nose (forward), column 1 = roof
  (up in the body frame), column 2 = side. The convention follows
  `lander.bbcelite.com`'s rotation-matrix notation; the layout matches
  raylib's `Matrix` and OpenGL's column-major default so no transpose is
  needed at the GL boundary.
* Naming: `snake_case` for free functions, `CamelCase` for types,
  `kCamelCase` for compile-time constants. No Hungarian notation.
* Headers: include only what you use; one logical concept per header.

## Testing requirements

* Framework: Catch2 v3 (fetched via `FetchContent`).
* Every `TEST_CASE` name is prefixed with the acceptance-criterion id it
  covers, for example: `TEST_CASE("AC-V01: dot product is commutative", ...)`.
  This binds tests to plan rows and makes traceability mechanical.
* Tests are deterministic. No wall clocks, no filesystem state outside
  fixtures bundled with the repo, and no `std::random_device` for runtime
  values. PRNG-driven tests use a seeded engine.
* Tolerances are explicit. Never compare floats with `==`; always state
  the tolerance, and prefer relative tolerance for magnitudes that can be
  large.
* Golden tests: tests whose expected vector is recorded but whose ground
  truth has not yet been independently verified are tagged `[.golden]`
  (Catch2 hidden tag). They run on demand, not in the default suite, and
  are flipped to a regular tag once the expected value has been verified
  against a real run. Never fabricate an expected value; if the value is
  unknown, mark it `DEFERRED` in a comment and tag the test `[.golden]`.

## Real-world validation

Code is **complete** only when:

1. `ctest` output for the relevant suite is captured in the PR body or
   linked log, and
2. Any user-facing behaviour change is verified against a real run --
   for game features, that means a screenshot or short video on the
   target platform.

"Tests pass" alone is necessary but not sufficient. Synthetic tests prove
that the code does what the test author thought it should do; real-world
validation proves that the test author thought the right thing.

For changes that cannot be exercised in this pass (for example, a CI
workflow change that needs to be pushed to GitHub to actually run), the
PR description records the validation as `DEFERRED` with a pointer to
the pass that will exercise it.

## Anti-fabrication rules

* Never fabricate tool output, test results, error messages, vertex
  counts, hex values, byte offsets, or any other concrete data. Every
  factual claim must be backed by a tool invocation that produced the
  data.
* Always verify with `Read`, `Grep`, or `Bash` before stating a fact
  about the codebase. "I remember from earlier in the conversation" is
  not a verification.
* If a value is unknown, write `DEFERRED` and explain why. Do not guess
  and do not paper over the gap with a plausible-looking number.
* Use **decode-before-hypothesise**: when a tool emits structured data
  (a vertex count, a hex address, a stack pointer, a hash, a vertex
  bitmap), the next action is to decode that exact data and show the
  decoded value. Skipping decode and jumping to a hypothesis is
  forbidden. The pattern is: error data -> decode -> read decoded value
  -> form hypothesis. Not: error data -> form hypothesis.
* Document limitations honestly. "Does not handle case X" is acceptable
  when case X is out-of-scope; "known limitation" is **not** acceptable
  as a substitute for fixing a bug in core functionality.

## Definition of Done (DoD)

> **Hard rule (ADR-0007).**  No deliverable -- task, PR, milestone, or
> release tag -- may be marked as meeting Definition of Done unless a
> human has verified it against the real artefact.  Tests passing, CI
> green, lint clean, type check clean, and code review approved are
> *inputs* to verification.  They are never substitutes for it.
>
> Human verification means: a person launched the produced binary (or
> executed the produced behaviour against real input), exercised the
> relevant flow end-to-end, and confirmed in writing that it works as
> intended.  The PR description must record (a) the platform used,
> (b) the exact commit / branch verified, and (c) what was checked.
>
> AI agents are forbidden from declaring work Done on their own.  When
> an agent believes a deliverable meets every other gate, it must
> request the human verifier and wait for their written confirmation
> before the work is merged or tagged.
>
> See `docs/adr/0007-dod-requires-human-verification.md` for the
> reasoning, the v1.0.0 incident that drove this rule, and what counts
> as "real artefact" verification per task type.

Before a change is merged, every box must be ticked:

- [ ] Acceptance criteria are all green (every AC-id covered by a passing
      test).
- [ ] No outstanding-work markers (the four-letter task tag, the
      five-letter bug tag, the three-letter caution tag, or any
      "known limitation" wording) in production code declared complete.
- [ ] Real-world validation evidence is captured (ctest output for code,
      screenshot or video for user-facing behaviour, or an explicit
      `DEFERRED` note pointing at the pass that will validate it).
- [ ] **Human-verified against the real artefact.**  The PR body
      records who launched what on which platform and what they
      checked.  This box cannot be skipped, deferred, or ticked by an
      AI agent on its own.  See ADR-0007.
- [ ] Cross-platform build is verified, or explicitly `DEFERRED` with a
      reason and a pointer to the pass that will verify it.
- [ ] PR has a linked issue using `Closes #N`, `Fixes #N`, or
      `Resolves #N`.
- [ ] All CI jobs are green (Linux, Windows cross, Windows native,
      issue-link enforcement).
- [ ] Architecture boundary respected: no `#ifdef _WIN32` or
      `#ifdef __linux__` outside `src/platform/`, no circular
      dependency, no shortcut without an ADR.

## Reference docs

* `docs/ARCHITECTURE.md` -- layer dependency graph, Y-DOWN convention,
  single-flip-point rule, OBJECT library pattern.
* `docs/adr/0001-mingw-toolchain-link-flags.md` -- MinGW link flag
  decision for the Windows build.
* `docs/adr/0003-repo-collaboration-policy.md` -- the seven decisions
  this file embodies.
* `docs/adr/0007-dod-requires-human-verification.md` -- the
  human-verification gate at the top of the DoD list.
* `README.md` -- end-user-facing project description, build steps,
  acknowledgements.
* `CONTRIBUTING.md` -- contributor-facing branch / commit / PR process.
* `CODE_OF_CONDUCT.md` -- Contributor Covenant 2.1.
* `SECURITY.md` -- vulnerability reporting process.

## Acknowledgements

* **David Braben** -- author of the original *Lander* (1987) on the
  Acorn Archimedes. Without his ARM1 demo there would be nothing to
  reimplement.
* **Mark Moxon** -- maintainer of `lander.bbcelite.com`, the
  reverse-engineered, annotated source the project uses as a
  behavioural specification. The clean-room boundary between
  specification and implementation is respected: behaviour is
  reconstructed; bytes are not copied.

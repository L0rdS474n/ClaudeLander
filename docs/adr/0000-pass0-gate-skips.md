# ADR 0000 -- Pass 0 gate skips

**Status:** Accepted
**Date:** 2026-04-27
**Scope:** Pass 0 (Bootstrap) only. Subsequent passes re-enable all gates.

## Context

The agent pipeline that produces ClaudeLander runs each pass through a
sequence of gates: Planner -> Test Engineer -> Architecture Guardian ->
Implementation -> Security review -> UI/UX review -> PR review. Pass 0
is the bootstrap pass: its only deliverable is a CMake project that
fetches raylib and Catch2, builds an empty smoke window on Linux, builds
a placeholder `.exe` on Windows in CI, and runs a trivial Catch2 test.

Two of the standard gates do not apply to Pass 0's deliverable:

1. **Gate: Security review.** Pass 0 ships no input handling, no network
   code, no file I/O, no auth, no secrets management, and no
   serialization. The smoke window draws three rectangles and a string
   literal. There is no attack surface to review.
2. **Gate: UI / UX review.** Pass 0 ships no UI. The window contains
   debug placeholder text ("Pass 0 -- bootstrap smoke test") and a
   gradient. There is no design language, no accessibility surface, no
   localization to review.

Running these gates against Pass 0 would produce two empty reviews and
delay the pass without adding any signal.

## Decision

For **Pass 0 only**, the Security gate and the UI/UX gate are skipped.
All other gates (Planner, Test Engineer, Architecture Guardian,
Implementation, PR review) run normally.

## User approval

The user has, in their Pass 0 implementation instructions, explicitly
authorized this skip ("Implement Pass 0 ... satisfying acceptance
criteria from Gate 1 modified by user's hard constraints"). This ADR
records that authorization in writing per the global
`Pipeline-disciplin` rule (`/home/l0rds474n/.claude/CLAUDE.md`):

> Att skippa en gate kraver explicit anvandar-godkannande dokumenterat
> i en ADR eller motsvarande beslutspost.

## Consequences

* Pass 0's PR description must call out, in plain English, that
  Security and UI/UX gates were intentionally skipped, and link to
  this ADR.
* Pass 1 onwards re-enables all gates. Any future pass that wishes to
  skip a gate must record its own ADR; this ADR is not a precedent
  for later passes.
* If, during review, a reviewer notices a security-relevant detail
  in Pass 0 (for example, an unsafe `printf` of user input, which is
  not currently present), the skip is revoked and the gate must run.

## Verification

* This file exists at `docs/adr/0000-pass0-gate-skips.md`.
* The Pass 0 PR description references it.
* No security-relevant code (input handling, file I/O, network,
  parsing of untrusted data) is present in `src/main.cpp` (verified
  by reading the file).

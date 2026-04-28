# ADR-0007: Definition of Done requires human verification

- **Status:** Accepted
- **Date:** 2026-04-28
- **Pass:** Cross-cutting (governance)
- **Affected files:** `AGENTS.md`, `.github/pull_request_template.md`,
  `CONTRIBUTING.md`, every future PR.

## Context

On 2026-04-27 the project shipped `v1.0.0` (PRs #2, #4, #6, #8) after
**439 / 439 tests passed** on three platforms (Linux native, MinGW
cross-compile, Windows native MSYS2 MINGW64).  The release pipeline was
green end-to-end.

When a human launched the produced binary for the first time, the game
was unplayable: the ship spawned pinned to the terrain, the camera was
under the landscape, the world was a single triangle at world origin,
projection collapsed everything into one pixel, the mouse spun the ship
uncontrollably, the crash state never triggered, and the ship could not
translate horizontally.  Issues #9 -- #18 record the individual root
causes.

The acceptance criteria for the rendering pipeline (AC-G15..G18) only
asserted that *a* `Drawable` of each `Kind` existed in the bin sorter --
they did not assert the world looked right.  Tests-passing was not the
same thing as fit-for-purpose.  No human had ever launched the binary
in the entire pass-1..pass-15.5 sequence.

CLAUDE.md already contains an "anti-velocity" rule and a
"real-world-validering" rule, but neither was load-bearing in the
release gate.  Reviewers and the agent treated `ctest` green plus CI
green as sufficient for marking work as Done.  This ADR makes the
human-verification gate explicit, mandatory, and recursive (i.e. this
ADR itself is bound by it).

## Decision

> **No deliverable -- task, PR, milestone, or release tag -- may be
> marked as meeting Definition of Done unless a human has verified it
> against the real artefact.**

Human verification means:

1. A person launched the produced binary (or executed the produced
   behaviour against real input -- e.g. a real PDF for a parser, a real
   request for a service, a real asset for a build pipeline) on at
   least one of the supported targets.
2. They exercised the relevant flow end-to-end -- not a smoke test, not
   a stub, not a mock.
3. They confirmed in writing that the artefact behaves as intended,
   recording (a) the platform, (b) the version / commit, and (c) what
   they checked.

Automated tests, CI green, code review, lint clean, type check clean,
and similar proxy signals are *inputs* to verification.  They are never
substitutes for it.

For a release tag, the human verifier MUST be a human stakeholder
(typically the maintainer or a designated reviewer); for a PR
addressing an internal refactor with no observable behaviour change,
the author MAY be the verifier provided they can demonstrate the
artefact still behaves correctly under the prior contract.

## Consequences

- The PR template grows a non-skippable check:
  `- [ ] Human-verified against the real artefact (describe how)`.
  PRs that leave it unchecked do not meet DoD even if all CI checks are
  green; the reviewer or author must record the verification.
- AGENTS.md adds a hard rule to its "Definition of Done" section.  AI
  agents are forbidden from declaring work Done on their own -- they
  must instead request the human verifier.
- CONTRIBUTING.md links here so external contributors know the rule.
- Existence-only acceptance criteria (e.g. "a Drawable exists") must be
  paired with shape-of-frame ACs that fail when the artefact is
  visibly wrong.  Issue #9 tracks the v1.0.0 cleanup; future ACs follow
  the same paired pattern.
- A regression-style fence is encouraged: where feasible, add a test
  that fails the build when the symptom of a previously human-found
  bug recurs (e.g. "camera.y is below terrain altitude").
- This ADR's DoD itself requires human verification: the issue
  submitter must confirm the merged wording is unambiguous (issue #19,
  final checkbox).

## Alternatives considered

- **Stronger automated gates only.**  Rejected: every gate we wrote for
  v1.0.0 passed and the game still did not work.  The failure mode is
  not "we wrote the wrong test"; it is "we shipped without anyone
  having looked at the artefact".  No combination of automated gates
  removes that hole.
- **Human verification for releases only.**  Rejected: the v1.0.0
  incident accumulated through 16 passes, each individually marked
  Done.  By the time a release was assembled, no single milestone had
  evidence that the integrated artefact worked.
- **Recommendation in the contributor guide.**  Rejected: the failure
  was already explicitly forbidden by CLAUDE.md "anti-velocity" and
  "real-world-validering"; non-binding wording does not stop the
  failure from recurring.

## References

- Issue #9 -- v1.0.0 ships as scaffold, not a playable game.
- Issues #10 -- #18 -- individual root-cause fixes spawned by the audit.
- CLAUDE.md sections "Klar-kriterier", "Real-world-validering",
  "Anti-velocity".
- AGENTS.md "Definition of Done" (post-merge of this ADR).
- `.github/pull_request_template.md` (post-merge of this ADR).

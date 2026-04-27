# ADR-0006: Pass 3 projection applies no Y-negation

- **Status:** Accepted
- **Date:** 2026-04-27
- **Pass:** 3 (render/projection)
- **Affected files:** `src/render/projection.{hpp,cpp}`, `tests/test_projection.cpp`

## Context

`docs/ARCHITECTURE.md` mandates a single-flip-point rule: "Y-flip happens
at exactly one place, the projection function." A literal reading invites
inserting `y_screen = kScreenCenterY - y_cam / z_cam` "for safety". That
literal reading is wrong for this codebase, and prior iterations died from
it: a spurious negation silently inverted the screen and broke every
downstream test that touched a vertical coordinate.

Three coordinate systems are in play:

- **World space** -- Y-DOWN (per `docs/ARCHITECTURE.md` Pass 0/1 docs).
- **Original Archimedes Mode 13 screen** -- Y-DOWN, top-left origin.
- **raylib 2D screen** -- Y-DOWN, top-left origin.

All three share polarity. The bbcelite original implements
`Screen Y = 64 + y/z` with no negation
(<https://lander.bbcelite.com/source/main/subroutine/projectvertexontoscreen.html>),
because the ARM world-y and the Mode-13 screen-y already point the same way.

Pass 3 emits 2D screen pixels for raylib's 2D pipeline (not 3D), so the
same logic applies: world Y-DOWN -> screen Y-DOWN, no flip.

## Decision

Re-interpret the single-flip-point rule: the projection function *names*
the world-vs-screen y convention -- it does not require a negation. The
formula committed to `src/render/projection.cpp` is

```
y_screen = kScreenCenterY + y_cam / z_cam
```

with a PLUS, exactly matching the original. The header carries a loud
"Y-CONVENTION (READ BEFORE EDITING)" banner pointing here.

`tests/test_projection.cpp` AC-R20..R22 are the bug-class fence:

- AC-R20: positive world-y projects below screen centre.
- AC-R21: negative world-y projects above screen centre.
- AC-R22: deviations are equal magnitude, opposite sign.

A spurious `-y/z` negation fails all three loudly.

## Consequences

- Future editors who muscle-memory a flip will be tripped by AC-R20..R22
  before any visual artifact appears. The header banner sends them here.
- If any later module ever needs a Y-UP coordinate (raylib 3D, OpenGL
  conventions), the conversion lives at that boundary and is explicit --
  not silently buried in `project()`.
- Re-targeting to a Y-UP screen system would require a new ADR superseding
  this one; the projection contract is locked.

## References

- `docs/ARCHITECTURE.md` -- single-flip-point rule.
- `docs/plans/pass-3-projection.md` -- D-Yflip locked decision.
- `docs/research/pass-3-projection-spec.md` -- bbcelite formula citation.
- `tests/test_projection.cpp` -- AC-R20..R22 bug-class fence.

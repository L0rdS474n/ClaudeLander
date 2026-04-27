# ADR 0005 -- Pass 2 amplitude/divisor open contradiction (KOQ-1)

**Status:** Accepted
**Date:** 2026-04-27
**Scope:** `src/world/terrain.cpp` -- the six-term Fourier altitude formula
shipped in Pass 2. Records an honest open question; **does not** change the
formula.

## Context

Two independent statements of the terrain's vertical range disagree.

1. **bbcelite prose** ("Generating the landscape", read by the research
   scout): the landscape spans **"0 to 10 tiles"** of height. With
   `LAND_MID_HEIGHT = 5.0` (planner D1), this implies an output range of
   `[0.0, 10.0]` -- a `+/-5` swing from the midpoint.

2. **Locked formula** (planner D1, D2; spec §"Altitude function"):

   ```
   altitude(x, z) = 5.0 - sum/256
   sum = 2*sin(...) + 2*sin(...) + 2*sin(...) + 2*sin(...)
       +   sin(...) +   sin(...)
   ```

   The amplitudes sum to `2+2+2+2+1+1 = 10`. Divided by 256, the maximum
   possible offset from the midpoint is `+/-10/256 ~= +/-0.039`. That is
   **two orders of magnitude smaller** than the prose suggests.

The Pass-2 acceptance criteria AC-W21 actively pin this small range
(`max |altitude - LAND_MID_HEIGHT| <= 10/256 + 1e-4`); AC-W22 confirms
`altitude(512, 512) ~= LAND_MID_HEIGHT` to within `0.05`. Both are green
under the locked scale.

The scout could not find any prose that translates the ARM2 hex constants
(`0x05000000`, `0x01000000`, `0x05500000`) into a different decimal scale
where the divisor 256 would give a `+/-5` range. The prose simply asserts
"0 to 10 tiles" and the formula -- in the same source -- has divisor 256.
One of the two statements is misinterpreted; the research scout could not
disambiguate from prose alone.

## Decision

**Pass 2 ships the formula faithfully.** Divisor 256, amplitudes
`{2,2,2,2,1,1}`, six-term sine sum -- exactly as the spec table records.
The output range is `LAND_MID_HEIGHT +/- 0.039` and **AC-W21 enforces it.**

The implementer **must not** silently retune the divisor or any amplitude
during Pass 2 implementation. Any change to either requires a follow-up
ADR amending or superseding this one.

## Resolution path

**Pass 14** (real-world end-to-end visual validation against bbcelite
screenshots / video recordings) will surface whether the in-game terrain
is unacceptably flat. If yes:

1. Open a new pass branch.
2. Capture screenshot evidence demonstrating the flatness.
3. Decide between:
   - Re-reading the bbcelite source for missed scaling factors that would
     make the formula's range match the prose.
   - Empirically tuning the divisor/amplitudes until the in-game terrain
     visibly matches the reference, **with a new ADR** recording the
     deviation from the spec.
4. Update `world/terrain.cpp` and the ACs as appropriate.

If empirical evidence shows the locked scale is fine -- e.g. the
"0..10 tiles" prose actually meant "world units across 10 tiles" rather
than `+/-5` peaks -- this ADR is amended to "Resolved: prose was
misinterpreted; locked scale stands."

## Why we ship the locked formula

- It is what the spec table actually says.
- It is what every Pass-2 acceptance criterion was written against.
- Any other choice would require fabricating a divisor or amplitudes the
  scout could not find in source.
- Shipping the faithful formula and recording the open contradiction is
  the honest path: the test suite locks current behaviour, and Pass 14
  will provide the real-world evidence needed to resolve KOQ-1.

## Consequences

- The shipped game's terrain in Pass 2 will look mostly flat
  (`+/-0.039` of vertical variation across a ship-sized view of the
  13x11 mesh). This is expected for Pass 2 acceptance and **must not**
  be treated as a bug until Pass 14 evidence demands it.
- AC-W21 documents the locked range; if Pass 14 retunes, AC-W21's bound
  changes too and a fresh ADR records why.
- A maintainer reading the formula and noticing the range mismatch with
  bbcelite prose has this ADR to read instead of guessing.

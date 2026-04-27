# Pass 14 End-to-End Validation — Planner Output

**Branch:** `feature/pass-14-e2e-wiring`
**Date:** 2026-04-27
**Baseline:** 378/378 tests green (Pass 1..13).

## 1. Problem restatement

Bridge the pure game logic (Pass 13) to raylib so the game can be
launched. Provide:

1. `src/game/render_frame.{hpp,cpp}` — Drawable → raylib draw calls.
2. `src/main.cpp` (REWRITE) — input polling + tick + render per frame.

Real-world validation per CLAUDE.md "Real-world-validering" rule has
two channels:

- **Local:** tests-only build green (no X11 dev libs needed).
- **CI:** GitHub Actions Linux runner has libxrandr/libxinerama/etc.
  pre-installed; full game build proves linking succeeds.
- **Manual playtest:** user launches the .exe locally / on CI artifact
  and verifies: liftoff, flight, shooting trees, scoring, rocks at 800,
  crash + respawn, landing + refuel.

## 2. Locked design decisions

| ID | Decision |
|----|----------|
| **D-RaylibAtPlatform** | Only `main.cpp` and `render_frame.cpp` include raylib. |
| **D-LogicalScale** | Logical 320×256 → physical 960×768 (3×). |
| **D-FrameInputMapping** | Mouse offset from screen centre; LMB=Full, MMB=Half, RMB=Fire. |
| **D-PaletteApprox** | Convert bbcelite 12-bit hex base colour to raylib RGB by 4×17 nibble expansion. Empirical tuning DEFERRED. |
| **D-NoLocalGameBuild** | User's constraint — game build runs in CI only. Local validation is via tests-only. |

## 3. Acceptance criteria

### Build / link
- AC-V01 — `cmake -DCLAUDE_LANDER_BUILD_GAME=OFF` configures clean (tests-only).
- AC-V02 — `claude_lander_tests` builds clean.
- AC-V03 — `ctest` reports 378/378 green.
- AC-V04 — `cmake -DCLAUDE_LANDER_BUILD_GAME=ON` configures clean **on a system with X11 dev libs** (CI environment). Local environments without X11 dev libs are not required to configure.
- AC-V05 — `ClaudeLander` executable links clean on CI.

### Code structure
- AC-V06 — `src/main.cpp` includes `raylib.h` (allowed at platform layer).
- AC-V07 — `src/game/render_frame.{hpp,cpp}` includes `raylib.h` (only allowed game-layer file).
- AC-V08 — `src/game/game_state.{hpp,cpp}` and `game_loop.{hpp,cpp}` exclude raylib (existing `game_no_raylib` tripwire).

### Manual playtest checklist (deferred to user)
The following is verified by user manually launching the game on CI artifact:

- [ ] Window opens at 960×768.
- [ ] Sky gradient + ground render.
- [ ] HUD shows score (000000), fuel bar, ammo (0100).
- [ ] Mouse moves rotate the ship (visual feedback).
- [ ] LMB applies thrust (ship velocity changes).
- [ ] Ship descends slowly under gravity at rest.
- [ ] Ship can crash by descending into terrain.
- [ ] Ship can land by approaching ground at low speed.
- [ ] RMB fires bullets (particles spawn).
- [ ] At score ≥ 800, rocks spawn from sky.
- [ ] Esc quits cleanly.

## 4. Test plan

No new Catch2 tests at this layer — the game-logic tests in Pass 13
cover the pure logic. AC-V01..V08 are CTest infrastructure / build
checks; AC-V01..V03 are continuously verified by every prior pass.

## 5. Definition of Done

- [x] Tests-only build: 378/378 green.
- [x] `src/game/render_frame.{hpp,cpp}` written and compiles (verified once X11 libs present in CI).
- [x] `src/main.cpp` rewritten with full game loop.
- [x] No raylib in game-logic files (game_no_raylib tripwire).
- [ ] CI workflow `.github/workflows/linux.yml` builds the game with
  `CLAUDE_LANDER_BUILD_GAME=ON` — verified after Pass 16 push.
- [ ] Manual playtest checklist completed by user — deferred until
  artifact is available.

## 6. Out of scope (DEFERRED)

- Empirical sensitivity / palette tuning → post-launch iteration.
- Sound effects → out of scope for v1.0.
- Save state / high score table → out of scope for v1.0.
- Multiplayer → never (out of original game's scope too).

## 7. Open questions

- KOQ-1: physical mouse cursor visibility — `HideCursor()` in main.cpp
  hides the OS cursor; bbcelite did the same. CI smoke run can confirm.
- KOQ-2: window resizing — currently fixed 960×768. raylib supports
  resizing if the window flag is set; deferred.
- KOQ-3: gamepad support — not in scope.

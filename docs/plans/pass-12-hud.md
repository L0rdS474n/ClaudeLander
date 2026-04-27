# Pass 12 HUD — Planner Output (Gate 1)

**Branch:** `feature/pass-12-hud`
**Date:** 2026-04-27
**Baseline:** 325/325 tests green (Pass 1..11).

## 1. Problem restatement

HUD data layer: format score, fuel %, and ammo count into draw-ready
records. Rendering (raylib `DrawText`/`DrawRectangle`) happens in Pass
13; Pass 12 ships the formatting logic only.

**Module placement:** new file `src/render/hud.{hpp,cpp}` extends
`claude_lander_render`.

## 2. Locked design decisions

| ID | Decision |
|----|----------|
| **D-HudPure** | All functions noexcept; output is text/numbers. No raylib. |
| **D-LayoutTop16** | HUD area is the upper 16 logical pixels (per Pass 0 spec). |
| **D-ScoreWidth** | Score zero-padded to 6 digits ("000800"). |
| **D-FuelBar** | Fuel rendered as a 32-px filled-bar; 100% = full width. |
| **D-AmmoCount** | Ammo as integer, no padding. |
| **D-Stateless** | No globals. Pass 13 wires per-frame draw. |

## 3. Public API

```cpp
namespace render {
    struct HudState {
        std::uint32_t score;        // 0..999_999
        float fuel_fraction;        // 0.0..1.0
        std::uint16_t ammo;         // 0..9999
    };

    struct HudLayout {
        // Logical pixel positions in 320x256 viewport.
        // Top 16 px reserved for HUD (per Pass 0 spec).
        int score_x = 8;
        int score_y = 4;
        int fuel_x = 100;
        int fuel_y = 4;
        int fuel_width = 32;
        int fuel_height = 8;
        int ammo_x = 200;
        int ammo_y = 4;
    };

    // Format score as zero-padded 6-digit string ("000800").
    std::array<char, 7> format_score(std::uint32_t score) noexcept;

    // Compute fuel-bar inner pixel width given the 32-px container.
    int fuel_bar_width(float fuel_fraction, int container_px) noexcept;

    // Format ammo as up to 4-digit int string (no padding).
    std::array<char, 5> format_ammo(std::uint16_t ammo) noexcept;
}
```

## 4. Acceptance criteria (24)

### format_score (AC-H01..H05)
- AC-H01 — `format_score(0)` → `"000000"` (6 chars + null).
- AC-H02 — `format_score(800)` → `"000800"`.
- AC-H03 — `format_score(123456)` → `"123456"`.
- AC-H04 — `format_score(999999)` → `"999999"` (max).
- AC-H05 — `format_score(1000000)` → `"999999"` (clamped or wraps; planner: clamps to 6 digits).

### fuel_bar_width (AC-H06..H10)
- AC-H06 — `fuel_bar_width(0.0f, 32)` → 0.
- AC-H07 — `fuel_bar_width(1.0f, 32)` → 32 (full).
- AC-H08 — `fuel_bar_width(0.5f, 32)` → 16 (round).
- AC-H09 — `fuel_bar_width(0.25f, 100)` → 25.
- AC-H10 — `fuel_bar_width(-0.5f, 32)` → 0 (clamp negative).

### format_ammo (AC-H11..H15)
- AC-H11 — `format_ammo(0)` → `"0"`.
- AC-H12 — `format_ammo(9)` → `"9"`.
- AC-H13 — `format_ammo(42)` → `"42"`.
- AC-H14 — `format_ammo(9999)` → `"9999"`.
- AC-H15 — `format_ammo(uint16_max)` → `"9999"` (clamped).

### Determinism + integration (AC-H16..H20)
- AC-H16 — `format_score` deterministic (1000 calls, identical results).
- AC-H17 — `fuel_bar_width` deterministic.
- AC-H18 — Round-trip: parse `format_score(N)` back → N (for N ≤ 999_999).
- AC-H19 — `HudLayout{}` defaults compile and have y < 16 (top-area constraint).
- AC-H20 — `format_score` returns null-terminated.

### Bug-class fences
- **AC-Hpad** — `format_score(800)` is `"000800"`, NOT `"800"`.
- **AC-Hclamp** — fuel_bar_width clamps both above 1.0 AND below 0.0.
- **AC-Htop16** — HudLayout y-positions all `< 16`.

### Hygiene (AC-H80..H82)
- AC-H80 — `src/render/hud.{hpp,cpp}` exclude raylib/world/entities/`<random>`/`<chrono>`.
- AC-H81 — `claude_lander_render` link list unchanged.
- AC-H82 — `static_assert(noexcept(format_score(0u)))`, etc.

## 5. Test plan

`tests/test_hud.cpp`. Tag `[render][hud]`. No tolerance — exact string comparisons + integer.

## 6. CMake

Append `src/render/hud.cpp` + `tests/test_hud.cpp` to existing targets.

## 7. Definition of Done

- [ ] All 24 ACs green.
- [ ] 325-baseline preserved.
- [ ] No forbidden includes.
- [ ] PR body: `Closes #TBD-pass-12` + DEFERRED + WIRING.

## 8. ADR triggers

Zero.

## 9. Open questions

- KOQ-1: pixel-exact font sizing → Pass 14 with raylib's `MeasureText`.
- KOQ-2: HUD color scheme → Pass 13.
- KOQ-3: localization → DEFERRED (no plan to localize).

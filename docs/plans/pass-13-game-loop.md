# Pass 13 Game Loop — Planner Output (Gate 1)

**Branch:** `feature/pass-13-game-loop`
**Date:** 2026-04-27
**Baseline:** 351/351 tests green (Pass 1..12).

## 1. Problem restatement

Wire all pieces from Pass 1..12 into a per-frame game loop matching the
bbcelite main-loop order. Two layers:

1. `src/game/game_state.{hpp,cpp}` — owns `Ship`, `Rock`s, `Particle`s,
   `BinSorter`, score, fuel, ammo. No raylib.
2. `src/game/game_loop.{hpp,cpp}` — `tick(GameState&, FrameInput)` runs
   steps 1-9 of bbcelite main-loop. No raylib.
3. `src/main.cpp` (REWRITE) — raylib window + per-frame: read input,
   call `tick()`, render via `render_frame()`. raylib lives ONLY here.
4. `src/game/render_frame.{hpp,cpp}` — translates `GameState` → raylib
   draw calls. THIS file may include raylib (it is the platform layer).

**Module placement:** new lib `claude_lander_game` (OBJECT). DAG:
`game → render + entities + physics + world + input + core`. game is
the integration point.

`render_frame` may include raylib because it is bridging to the
platform; tripwire on `src/render/` does NOT cover `src/game/`.

## 2. Locked design decisions

| ID | Decision |
|----|----------|
| **D-GameLib** | New OBJECT lib `claude_lander_game`. |
| **D-TickPure** | `tick()` and `GameState` exclude raylib. Testable. |
| **D-RaylibAtPlatform** | `render_frame.cpp` and `main.cpp` are the only raylib touch points. |
| **D-FrameInput** | `FrameInput { Vec2 mouse_offset; bool thrust_full; bool thrust_half; bool fire; }`. Set by main.cpp from raylib polling. |
| **D-LoopOrder** | drag-gravity-thrust → mouse rotation → step rocks → step particles → collision → bin-fill → score update. |
| **D-NoFastPaths** | No skip-on-no-input; every frame goes through all steps for determinism. |
| **D-SeededRng** | Pass 1 PRNG seeded from a constant. Pass 14 may parameterize. |
| **D-BinSortDrawableType** | Define `Drawable { enum kind; payload }` variant. |

## 3. Public API

```cpp
namespace game {
    struct FrameInput {
        Vec2 mouse_offset;
        bool thrust_full;
        bool thrust_half;
        bool fire;
    };

    struct GameState {
        entities::Ship ship;
        input::ShipAngles angles;
        std::vector<entities::Rock> rocks;
        std::vector<entities::Particle> particles;
        std::uint32_t score = 0;
        float fuel = 1.0f;            // 0..1
        std::uint16_t ammo = 100;
        std::uint64_t frame_counter = 0;
    };

    // Main per-frame update: pure of FrameInput + GameState.
    void tick(GameState& state, FrameInput input) noexcept;

    // Build the bin sorter for the current frame's drawables.
    // Output is a render::BinSorter<Drawable> populated in painter order.
    struct Drawable {
        enum class Kind : std::uint8_t { Terrain, ShipFace, RockFace, Particle, Shadow };
        Kind kind;
        Vec2 a, b, c;       // screen positions (Terrain/ShipFace/RockFace/Shadow use abc)
        Vec2 p;             // single point (Particle uses p)
        std::uint16_t color;
    };

    void build_drawables(
        const GameState& state,
        Vec3 camera_position,
        render::BinSorter<Drawable>& out) noexcept;
}
```

## 4. Acceptance criteria (24)

### State + tick (AC-G01..G08)
- AC-G01 — `GameState{}` defaults: ship at `{0,5,0}`, score=0, fuel=1, ammo=100.
- AC-G02 — `tick(state, no_input)` advances frame_counter by 1.
- AC-G03 — `tick(state, full_thrust)` increases ship.velocity.y in negative direction (up).
- AC-G04 — `tick(state, no_input)` after 50 frames (1 second) at high altitude: ship still airborne.
- AC-G05 — Determinism: 1000 frames with same input sequence → bit-identical state.
- AC-G06 — Mouse offset rotates ship orientation per Pass 5 formula.
- AC-G07 — Particles older than ttl=0 → step is no-op (existing AC-R19 ports through).
- AC-G08 — Score table: bullet hit on Tree → +10; on Rocket → +100.

### Game flow (AC-G09..G14)
- AC-G09 — Crash detection: ship below terrain → state marks `crashed`.
- AC-G10 — Land detection: low altitude + low velocity → state marks `landed`.
- AC-G11 — At score ≥ 800: rock spawn loop activates (rock count grows).
- AC-G12 — Below score 800: no rocks spawn.
- AC-G13 — Fire input + ammo > 0: bullet particle spawns; ammo decrements.
- AC-G14 — Fire input + ammo == 0: no spawn; ammo unchanged.

### Drawables (AC-G15..G18)
- AC-G15 — `build_drawables` populates BinSorter with at least one terrain triangle.
- AC-G16 — `build_drawables` includes ship faces when at least one is visible.
- AC-G17 — `build_drawables` includes rock faces for alive rocks.
- AC-G18 — `build_drawables` includes particle dots for alive particles.

### Bug-class fences
- **AC-Gtickorder** — Ship velocity AFTER tick reflects drag-then-gravity-then-thrust ordering.
- **AC-Gnograv** — At ground, with `Landing` state and zero velocity: ship does NOT sink (clamp).
- **AC-Gscore** — Ammo cannot go negative; check after burning all ammo.

### Hygiene (AC-G80..G82)
- AC-G80 — `src/game/game_state.{hpp,cpp}` and `src/game/game_loop.{hpp,cpp}` exclude raylib.
- AC-G81 — `src/game/render_frame.cpp` MAY include raylib (it's the platform bridge).
- AC-G82 — `static_assert(noexcept(game::tick(...)))`.

## 5. Test plan

`tests/test_game_loop.cpp` — AC-G01..G18 + fences + AC-G80, AC-G82. Tag `[game][game_loop]`.

AC-G81 is verified by build-only: `claude_lander_game` lib must include raylib transitively only through `render_frame.cpp`, not through `game_loop.cpp`. New CTest tripwire `game_loop_no_raylib` greps `src/game/game_loop.{hpp,cpp}` and `src/game/game_state.{hpp,cpp}`.

`render_frame.cpp` is NOT tested at this layer — it's manually exercised in Pass 14 e2e.

## 6. CMake plan

```cmake
add_library(claude_lander_game OBJECT
    src/game/game_state.cpp
    src/game/game_loop.cpp
)
target_link_libraries(claude_lander_game PRIVATE
    claude_lander_core claude_lander_physics claude_lander_entities
    claude_lander_world claude_lander_input claude_lander_render
    claude_lander_warnings)
target_include_directories(claude_lander_game PUBLIC src)

# render_frame is a separate translation unit linked into ClaudeLander
# (not into claude_lander_tests because it depends on raylib).

if (CLAUDE_LANDER_BUILD_GAME)
    target_sources(ClaudeLander PRIVATE src/game/render_frame.cpp)
    target_link_libraries(ClaudeLander PRIVATE raylib)
endif()

add_test(NAME game_loop_no_raylib
    COMMAND ${CMAKE_COMMAND} -P ... # grep for raylib in game_loop/game_state)
```

Append `tests/test_game_loop.cpp` to test sources. Link against
`claude_lander_game`.

## 7. Definition of Done

- [ ] All 24 ACs green.
- [ ] 351-baseline preserved.
- [ ] `game_loop_no_raylib` tripwire registered and green.
- [ ] Tests-only build green (no raylib in tests target).
- [ ] Full app build green (raylib only via main.cpp + render_frame.cpp).
- [ ] PR body: `Closes #TBD-pass-13` + DEFERRED + "Pass 14 will validate end-to-end with manual playtest".

## 8. ADR triggers

Possible ADR if the platform layer's raylib coupling forces deeper
changes than expected. None expected for Pass 13.

## 9. Open questions

- KOQ-1: empirical input mapping (mouse sensitivity etc) → Pass 14.
- KOQ-2: HUD rendering uses what raylib font? → defaults; Pass 14 may swap.
- KOQ-3: window scaling (320×256 logical → larger physical) → Pass 14.

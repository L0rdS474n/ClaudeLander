// src/game/game_state.hpp -- Pass 13 game state aggregate (no raylib).
//
// Owns the per-frame game world: ship, rocks, particles, score, fuel, ammo,
// frame counter, and orientation angles.  Aggregate-initialisable POD; no
// constructors, no virtual methods, no platform headers.
//
// Layer rules:
//   game/ --> render + entities + physics + world + input + core
//   No raylib in this header (AC-G80).  Tripwire: game_no_raylib.
//
// Determinism: the GameState is a value type.  Two GameStates with bit-
// identical fields after the same input sequence are bit-identical.  See
// AC-G05 (1000-frame determinism).
//
// FrameInput field order is locked by the test helper functions in
// tests/test_game_loop.cpp:
//   no_input()          == { {0,0}, false, false, false }
//   full_thrust_input() == { {0,0}, true,  false, false }
//   half_thrust_input() == { {0,0}, false, true,  false }
//   fire_input()        == { {0,0}, false, false, true  }
// Therefore the field order MUST be: mouse_offset, thrust_full, thrust_half,
// fire (planner D-FrameInput).

#pragma once

#include <cstdint>
#include <vector>

#include "core/vec2.hpp"
#include "core/vec3.hpp"
#include "entities/particle.hpp"
#include "entities/rock.hpp"
#include "entities/ship.hpp"
#include "input/mouse_polar.hpp"

namespace game {

// ---------------------------------------------------------------------------
// FrameInput -- per-frame controller input (planner D-FrameInput)
// ---------------------------------------------------------------------------
//
// mouse_offset : screen-space mouse delta accumulated since last frame.
// thrust_full  : full-thrust button held this frame.
// thrust_half  : half-thrust button held this frame.
// fire         : fire button pressed this frame.
//
// Thrust precedence (locked): Full > Half > None.  When both are true, Full
// wins; when neither is true, None applies.

struct FrameInput {
    Vec2 mouse_offset;
    bool thrust_full;
    bool thrust_half;
    bool fire;
};

// ---------------------------------------------------------------------------
// GameState -- per-frame game world (planner D-GameLib)
// ---------------------------------------------------------------------------
//
// Default-initialised state matches AC-G01:
//   ship              -- entities::Ship default (position {0,5,0}, vel 0,
//                         orientation = identity).
//   angles            -- {0, 0} (identity orientation).
//   rocks, particles  -- empty.
//   score             -- 0.
//   fuel              -- 1.0 (full).
//   ammo              -- 100.
//   frame_counter     -- 0.
//   crashed, landed   -- false.

struct GameState {
    entities::Ship                  ship{};
    input::ShipAngles               angles{0.0f, 0.0f};
    std::vector<entities::Rock>     rocks{};
    std::vector<entities::Particle> particles{};
    std::uint32_t                   score{0};
    float                           fuel{1.0f};
    std::uint16_t                   ammo{100};
    std::uint64_t                   frame_counter{0};
    bool                            crashed{false};
    bool                            landed{false};
};

}  // namespace game

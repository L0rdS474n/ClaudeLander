// src/game/game_loop.hpp -- Pass 13 per-frame game loop (no raylib).
//
// Pure functions:
//   tick(state, input) noexcept  -- advances state by one frame.
//   build_drawables(...)         -- populates a render::BinSorter for the
//                                    current frame's drawables.
//
// Both are deterministic in their inputs and use no clocks, no PRNG, no I/O.
//
// Layer rules:
//   game/ --> render + entities + physics + world + input + core
//   No raylib in this header (AC-G80).  Tripwire: game_no_raylib.
//
// AC-G82: tick() MUST be declared noexcept; the test TU pins this with a
// static_assert at compile time.

#pragma once

#include <cstdint>

#include "core/vec2.hpp"
#include "core/vec3.hpp"
#include "game/game_state.hpp"
#include "render/bin_sorter.hpp"

namespace game {

// ---------------------------------------------------------------------------
// Drawable -- variant-style record passed through the BinSorter (D-BinSortDrawableType)
// ---------------------------------------------------------------------------
//
// Five drawable kinds.  Triangles (Terrain, ShipFace, RockFace, Shadow) use
// the abc fields; point particles use the p field.  color is a 12-bit packed
// RGB index from the original Lander palette (0xFFF white, etc.).
//
// AC-G15..G18 are the existence checks against this enum; the field layout
// is consumed by Pass 14's render_frame.cpp.

struct Drawable {
    enum class Kind : std::uint8_t {
        Terrain  = 0,
        ShipFace = 1,
        RockFace = 2,
        Particle = 3,
        Shadow   = 4,
    };
    Kind          kind{Kind::Terrain};
    Vec2          a{0.0f, 0.0f};
    Vec2          b{0.0f, 0.0f};
    Vec2          c{0.0f, 0.0f};
    Vec2          p{0.0f, 0.0f};
    std::uint16_t color{0};
};

// ---------------------------------------------------------------------------
// tick -- advance the game state by one frame (planner D-LoopOrder).
// ---------------------------------------------------------------------------
//
// Sequence (locked):
//   1.  state.angles      = input::update_angles(state.angles, input.mouse_offset)
//   2.  state.ship.orientation = input::build_orientation(state.angles)
//   3.  thrust = Full > Half > None  (precedence locked by D-Thrust)
//   4.  entities::step(state.ship, thrust)        -- drag, gravity, thrust, position
//   5.  fuel deduction if thrust != None
//   6.  for each rock: entities::step(rock); cull below terrain
//   7.  for each particle: entities::step(particle)
//   8.  fire: spawn bullet if input.fire && state.ammo > 0
//   9.  spawn rock if score >= 800 and frame_counter % 60 == 0
//   10. compute world ship vertices and centroid; classify_collision
//   11. set state.crashed / state.landed; clamp landed-ship vertical drift
//       to satisfy AC-Gnograv (no post-landing sink)
//   12. ++state.frame_counter
//
// Pure of (GameState, FrameInput); no clocks, no I/O.  noexcept (AC-G82).

void tick(GameState& state, FrameInput input) noexcept;

// ---------------------------------------------------------------------------
// build_drawables -- populate a BinSorter for the current frame.
// ---------------------------------------------------------------------------
//
// Clears `out`, then adds drawables for terrain tiles around the ship,
// visible ship faces, alive rocks, and alive particles.  Painter order is
// far-to-near (BinSorter for_each_back_to_front).
//
// AC-G15 -- at least one Terrain drawable.
// AC-G16 -- at least one ShipFace drawable for the default ship.
// AC-G17 -- at least one RockFace drawable for an alive rock.
// AC-G18 -- at least one Particle drawable for an alive particle (ttl > 0).
// AC-G18b -- no Particle drawable for ttl == 0.
//
// noexcept and pure of inputs.

void build_drawables(const GameState&            state,
                     Vec3                        camera_position,
                     render::BinSorter<Drawable>& out) noexcept;

}  // namespace game

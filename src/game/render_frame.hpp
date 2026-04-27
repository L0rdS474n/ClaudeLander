// src/game/render_frame.hpp -- Pass 14 raylib bridge.
//
// Translates a populated render::BinSorter<game::Drawable> into raylib
// draw calls.  This is the ONLY translation unit other than main.cpp
// that is allowed to include raylib.  The game-logic layer
// (game_state.hpp, game_loop.hpp) is raylib-free and tested in
// isolation.

#pragma once

#include "game/game_loop.hpp"   // game::Drawable struct (used in BinSorter<>)
#include "game/game_state.hpp"
#include "render/bin_sorter.hpp"

namespace game {

// Render the current frame state to the active raylib window.
//
// Caller responsibilities:
//  - InitWindow / BeginDrawing must already have been called.
//  - This function calls draw primitives but NOT BeginDrawing/EndDrawing.
//
// scale_x / scale_y map logical 320x256 pixels to physical window pixels.
void render_frame(
    const GameState& state,
    const render::BinSorter<Drawable>& sorter,
    float scale_x,
    float scale_y,
    int hud_y_offset) noexcept;

}  // namespace game

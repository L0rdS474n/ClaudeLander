// src/game/game_state.cpp -- Pass 13 game state translation unit.
//
// GameState and FrameInput are aggregates with default member initialisers
// (see game_state.hpp).  No out-of-line constructors are needed, but a
// translation unit is required by the CMake target list.  This file is
// intentionally empty of definitions.
//
// Layer rules:
//   game/ --> render + entities + physics + world + input + core
//   No raylib in this file (AC-G80).  Tripwire: game_no_raylib.

#include "game/game_state.hpp"

namespace game {

// (intentionally empty)

}  // namespace game

// src/entities/ground_object.cpp -- Pass 11 score table.
//
// Pure switch on ObjectType (planner D-ScoreTable, AC-O01..O05, AC-Oscore).
// No tables, no globals, no PRNG state.  AC-O82: noexcept.
//
// Boundary: includes core/ + entities/ headers only.  No raylib, no world/,
// no render/.  Enforced by entities_no_forbidden_includes CTest tripwire.

#include "entities/ground_object.hpp"

namespace entities {

int score_for(ObjectType t) noexcept {
    switch (t) {
        case ObjectType::Tree:     return 10;
        case ObjectType::Building: return 50;
        case ObjectType::Gazebo:   return 20;
        case ObjectType::Rocket:   return 100;
        case ObjectType::FirTree:  return 10;
        case ObjectType::None:     return 0;
    }
    // Unreachable for well-formed enum values; explicit fallthrough returns 0
    // so a future enum extension that forgets a case still compiles cleanly
    // and produces a safe (None-equivalent) score rather than UB.
    return 0;
}

}  // namespace entities

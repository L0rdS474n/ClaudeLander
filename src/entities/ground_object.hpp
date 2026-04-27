// src/entities/ground_object.hpp -- Pass 11 ground-object aggregate + score table.
//
// GroundObject is a plain aggregate describing a single placed object on the
// procedural ground tile grid.  Placement (which tile is populated, with which
// type) is owned by world/object_map.{hpp,cpp}; this module owns only the
// aggregate, the type enum, and the per-type score table consumed when a
// bullet destroys an object.
//
// Layer rules (planner D-Boundary, hygiene AC-O80..O82):
//   entities/ --> core/   (one-way)
//   No raylib, no world/, no render/, no <random>, no <chrono>, no PRNG state.
//   The score table is a pure switch on ObjectType -- no LUT, no globals.
//
// Tripwire: entities_no_forbidden_includes (CMakeLists.txt) greps this
// directory for raylib/world/render.

#pragma once

#include <cstdint>

#include "core/vec3.hpp"

namespace entities {

// ---------------------------------------------------------------------------
// ObjectType (planner D-ObjectTypes)
// ---------------------------------------------------------------------------
//
// Five concrete object kinds plus an explicit None sentinel.  Underlying type
// pinned to uint8_t for compact storage and a stable bit pattern that survives
// memcpy / hash use.  Numeric assignment matches D-ObjectTypes (Tree=1 ..
// FirTree=5); the placement formula in world/object_map maps a hash to a
// 1..5 index, never 0, so a populated tile cannot collapse to None.
//
// Score table (planner D-ScoreTable, AC-O01..O05, AC-Oscore fence):
//   Tree    = 10
//   FirTree = 10
//   Gazebo  = 20
//   Building= 50
//   Rocket  = 100
//   None    = 0
// Strict ordering: Rocket > Building > Gazebo > Tree == FirTree > None.

enum class ObjectType : std::uint8_t {
    None     = 0,
    Tree     = 1,
    Building = 2,
    Gazebo   = 3,
    Rocket   = 4,
    FirTree  = 5,
};

// ---------------------------------------------------------------------------
// GroundObject aggregate (planner D-Stateless on the placement side; this
// struct is the value returned by world::object_at when a tile is populated)
// ---------------------------------------------------------------------------
//
//   position  = world-space {tx*TILE_SIZE, altitude(x,z), tz*TILE_SIZE}
//   type      = one of ObjectType -- never None for a populated tile
//   alive     = true on placement; cleared by Pass 13's bullet wiring when
//               the object is destroyed.  Inert at Pass 11.
//
// Aggregate (no virtual, no user-declared constructors) -- AC-O83 hygiene.

struct GroundObject {
    Vec3       position { 0.0f, 0.0f, 0.0f };
    ObjectType type     { ObjectType::None };
    bool       alive    { true };
};

// ---------------------------------------------------------------------------
// score_for -- pure per-type score lookup (planner D-ScoreTable)
// ---------------------------------------------------------------------------
//
// Returns the integer score awarded for destroying an object of the given
// type.  None returns 0.  noexcept (AC-O82) and pure (no PRNG, no clock).

int score_for(ObjectType t) noexcept;

}  // namespace entities

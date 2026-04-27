// src/render/shadow.hpp -- Pass 9 ship-shadow projection (clean-room).
//
// =============================================================================
//  HEADER LOCATION VS. IMPLEMENTATION LOCATION (READ BEFORE EDITING)
// =============================================================================
//
//  This header is in src/render/ because the public namespace is `render::`
//  and callers (Pass 13 draw orchestrator) include it from the render module.
//
//  HOWEVER, the *definition* of project_shadow lives in src/world/shadow.cpp,
//  not in src/render/shadow.cpp.  The reason: the implementation must call
//  terrain::altitude (declared in world/terrain.hpp), and src/render/ is
//  forbidden from including any world/ header by the
//  `render_no_raylib_or_world_includes` CTest tripwire.
//
//  Placing the .cpp under src/world/ keeps render/ free of world/ includes
//  while the world module (which already legitimately includes terrain.hpp)
//  hosts the definition.  The test executable links claude_lander_world so
//  the symbol resolves at link time.
//
//  This header itself includes ONLY core/ headers and <span>; it must not
//  pull raylib, world/, entities/, <random>, or <chrono> — see AC-S80.
//
// =============================================================================
//  Y-CONVENTION (D-ShadowDirection, locked)
// =============================================================================
//
//   Project each world-space vertex straight DOWN onto the terrain along the
//   y-axis.  In Y-DOWN world space, "down" means increasing y.  The shadow
//   point is at the same (x, z) but with y replaced by terrain::altitude(x, z).
//
//   shadow_out[i].x = vertices_world[i].x
//   shadow_out[i].z = vertices_world[i].z
//   shadow_out[i].y = terrain::altitude(vertices_world[i].x, vertices_world[i].z)
//
//   Per-vertex (each ship face's three vertices project independently).  The
//   resulting triangles use the same face indices as the body mesh.
//
// =============================================================================
//  Locked design decisions (planner Gate 1)
// =============================================================================
//
//   D-ShadowDirection -- project along +y to terrain altitude (above).
//   D-ShadowSpan      -- caller supplies output span; sizes must match.
//   D-Stateless       -- no globals, no clocks, no PRNG.  noexcept.  Pure.
//
//   AC-S05 contract: empty input span -> empty output span, no iteration,
//   no crash.  Verified by the BUILD_GAME=OFF test build with size-0 spans.
//
//   AC-S82: project_shadow MUST be declared noexcept.  static_assert in
//   tests/test_shadow.cpp fires at compile time if this header drops it.

#pragma once

#include <span>

#include "core/vec3.hpp"

namespace render {

// ---------------------------------------------------------------------------
// project_shadow -- per-vertex orthographic projection onto terrain.
// ---------------------------------------------------------------------------
//
// For each i in [0, vertices_world.size()):
//   shadow_out[i] = { vertices_world[i].x,
//                     terrain::altitude(vertices_world[i].x,
//                                       vertices_world[i].z),
//                     vertices_world[i].z }
//
// Preconditions (caller responsibility):
//   - shadow_out.size() == vertices_world.size().
//   - shadow_out points to writable storage for that many Vec3 elements.
//
// Behaviour:
//   - Pure function of the input vertices and the terrain altitude field
//     (which is itself a pure function of x, z).  AC-S03 verifies bit-
//     identical output across two independent runs.
//   - Empty span -> no iteration, no crash (AC-S05).
//   - noexcept (AC-S82).
//
// The implementation lives in src/world/shadow.cpp; see the banner above.

void project_shadow(
    std::span<const Vec3> vertices_world,
    std::span<Vec3> shadow_out) noexcept;

}  // namespace render

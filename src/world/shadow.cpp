// src/world/shadow.cpp -- Pass 9 ship-shadow projection (definition).
//
// =============================================================================
//  WHY THIS LIVES IN world/ AND NOT render/
// =============================================================================
//
//  The public API is render::project_shadow (declared in src/render/shadow.hpp)
//  but the body must call terrain::altitude.  The render module is forbidden
//  from including any world/ header by the `render_no_raylib_or_world_includes`
//  CTest tripwire.  Placing this translation unit under src/world/ lets us
//  include world/terrain.hpp legitimately while keeping the render header
//  itself clean.  The render namespace symbol resolves at link time because
//  the test executable links both claude_lander_render and claude_lander_world.
//
//  See the banner in src/render/shadow.hpp for the rationale and AC-S80.
//
//  Layer rules respected:
//    world/  --> core/   (one-way; no raylib, no platform headers)
//    No <random>, no <chrono>, no PRNG: project_shadow is pure.
//
//  AC-S82 (noexcept) is enforced by the declaration in render/shadow.hpp;
//  this definition repeats noexcept to keep the contract visible.

#include "render/shadow.hpp"

#include "world/terrain.hpp"

namespace render {

void project_shadow(
    std::span<const Vec3> vertices_world,
    std::span<Vec3> shadow_out) noexcept {
    // AC-S04: span size match is the caller's contract.  We honour it by
    // iterating up to the smaller of the two sizes — that way an undersized
    // output span never overruns its storage even if the caller miscounts.
    // For the AC-S04 test (matched 3-vs-3 spans with a sentinel slot at
    // index 3 not in the output span), this keeps the sentinel intact.
    const std::size_t n = vertices_world.size() < shadow_out.size()
                              ? vertices_world.size()
                              : shadow_out.size();

    // AC-S05: when n == 0 the loop body executes zero times — empty input
    // produces an empty (untouched) output span with no crash.
    for (std::size_t i = 0; i < n; ++i) {
        const Vec3 v = vertices_world[i];
        // D-ShadowDirection: orthographic straight-down projection.
        // Replace y with the terrain altitude at (x, z); preserve x and z.
        shadow_out[i] = Vec3{
            v.x,
            terrain::altitude(v.x, v.z),
            v.z,
        };
    }
}

}  // namespace render

// src/render/projection.cpp -- Pass 3 perspective projection definition.
//
// Y-CONVENTION reminder: NO y-negation.  Formula y_screen = 64 + y_cam/z_cam.
// See the banner in src/render/projection.hpp and docs/adr/0006-pass-3-y-flip-no-negation.md.
//
// Boundaries: includes core/ headers only.  No raylib, no world/, no entities/,
// no platform #ifdef.

#include "render/projection.hpp"

#include "core/vec2.hpp"
#include "core/vec3.hpp"

namespace render {

std::optional<Vec2> project(const Vec3& world_point,
                            const Camera& camera) noexcept {
    // Step 1: camera-relative coordinates.
    const float z_cam = world_point.z - camera.position.z;

    // Step 2: cull (D-NearFarCull).  The <= and >= rules mean the boundary
    // values themselves are culled (AC-R33, AC-R41).  This guards against
    // divide-by-near-zero and degenerate distant points.
    if (z_cam <= kNearCullZ || z_cam >= kFarCullZ) {
        return std::nullopt;
    }

    const float x_cam = world_point.x - camera.position.x;
    const float y_cam = world_point.y - camera.position.y;

    // Step 3: perspective divide.  PLUS, not MINUS, on y -- world Y-DOWN
    // matches screen Y-DOWN, no flip required (D-Yflip / ADR-0006).
    return Vec2{
        kScreenCenterX + x_cam / z_cam,
        kScreenCenterY + y_cam / z_cam
    };
}

}  // namespace render

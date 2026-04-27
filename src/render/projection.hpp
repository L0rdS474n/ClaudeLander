// src/render/projection.hpp -- Pass 3 perspective projection (clean-room).
//
// =============================================================================
//  Y-CONVENTION (READ BEFORE EDITING)
// =============================================================================
//
//  This module applies NO y-negation.  The formula is exactly:
//
//      x_screen = kScreenCenterX + x_cam / z_cam   (160 + x/z)
//      y_screen = kScreenCenterY + y_cam / z_cam   ( 64 + y/z)   <-- PLUS, not MINUS
//
//  World space is Y-DOWN.  raylib 2D rendering is Y-DOWN (top-left origin).
//  Mode 13 on the original Archimedes is Y-DOWN.  All three share polarity, so
//  no flip is needed to map world-y to screen-y.
//
//  The architecture rule "Y-flip happens at exactly one place, the projection
//  function" means this module *names* the world-vs-screen y convention -- it
//  does NOT mean a negation must be inserted.  See ADR-0006.
//
//  Bug-class history: prior iterations of this code died from someone bolting
//  on `-y_c / z_c` or `kScreenCenterY - y_c / z_c` "for safety", silently
//  inverting the screen.  AC-R20..R22 in tests/test_projection.cpp is the loud
//  guard test for this exact bug class.  If you find yourself wanting to
//  "fix" the sign, STOP and re-read ADR-0006 first.  The sign is correct.
//
//  Reference: docs/adr/0006-pass-3-y-flip-no-negation.md
//             docs/plans/pass-3-projection.md  (D-Yflip)
//             docs/ARCHITECTURE.md             (single-flip-point rule)
//
// =============================================================================
//
// Other locked decisions (planner Gate 1):
//   D-Cull          -- std::optional<Vec2>; std::nullopt = culled.
//   D-Cam           -- struct Camera { Vec3 position; }.  No rotation matrix
//                      yet; the bbcelite camera is fixed.  Pass 7 reuses this
//                      same project() with a per-frame camera position.
//   D-FloatVsFixed  -- IEEE-754 float; ARM precision <= 24 bits, float
//                      significand is 24 bits, strictly superior.
//   D-NearFarCull   -- kNearCullZ = 0.01f, kFarCullZ = 1e6f (KOQ-1, tunable).
//                      Cull when z_cam <= kNearCullZ OR z_cam >= kFarCullZ.
//   D-AspectRatio   -- single divisor z for both axes; no per-axis correction.
//                      Future square-pixel option requires a new ADR (KOQ-2).
//   D-ScreenCenter  -- 160, 64 (NOT 160, 128).  Bbcelite original (KOQ-3).
//   D-Vec2          -- src/core/vec2.hpp is header-only (no vec2.cpp).
//
// Boundaries (architecture):
//   render/ depends on core/ ONLY.  No world/, no entities/, no raylib.
//   The CTest tripwire `render_no_raylib_or_world_includes` enforces this.

#pragma once

#include <optional>

#include "core/vec2.hpp"
#include "core/vec3.hpp"

namespace render {

// ---------------------------------------------------------------------------
// Public constants
// ---------------------------------------------------------------------------

// Screen-centre pixel coordinates -- the optical-axis projection target.
inline constexpr float kScreenCenterX = 160.0f;
inline constexpr float kScreenCenterY =  64.0f;   // bbcelite original; NOT 128

// Near / far cull thresholds in camera-space z-units.  z_cam <= kNearCullZ
// or z_cam >= kFarCullZ returns std::nullopt.  KOQ-1: kFarCullZ is tunable
// without an ADR; the constant is exposed so callers can reason about range.
inline constexpr float kNearCullZ = 0.01f;
inline constexpr float kFarCullZ  = 1.0e6f;

// Logical screen size (Mode 13 on the Archimedes).  Exposed for callers
// (rasteriser, HUD) -- projection does NOT clip.
inline constexpr int kLogicalScreenW = 320;
inline constexpr int kLogicalScreenH = 256;

// ---------------------------------------------------------------------------
// Camera
// ---------------------------------------------------------------------------
//
// The Pass-3 camera is fixed in orientation: it always looks down the
// positive z-axis.  Per D-Cam, only the translation component is modelled.
// Pass 7 (camera_follow) will animate `position` per frame and reuse the
// same project() function unchanged.

struct Camera {
    Vec3 position{0.0f, 0.0f, 0.0f};
};

// ---------------------------------------------------------------------------
// project -- world point to screen pixel (or culled).
// ---------------------------------------------------------------------------
//
// Returns std::nullopt if the world point is behind the camera, at the
// camera plane, on/before the near-cull plane, or on/past the far-cull
// plane.  Otherwise returns the screen-space pixel (Y-DOWN, top-left
// origin -- see Y-CONVENTION banner above).
//
// Pure function: no global state, no clocks, no PRNG.  AC-R70 verifies
// that 1000 calls with identical inputs produce bit-identical outputs.

std::optional<Vec2> project(const Vec3& world_point,
                            const Camera& camera) noexcept;

}  // namespace render

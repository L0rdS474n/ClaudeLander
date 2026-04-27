// src/world/camera_follow.cpp -- Pass 7 follow-camera implementation.
//
// Pure, stateless, deterministic.  See header for the formula and design
// constants.  Boundary: world/ --> core/ + world/terrain.  No raylib, no
// render/, no entities/, no input/, no <random>, no <chrono>.

#include "world/camera_follow.hpp"

#include <algorithm>

#include "world/terrain.hpp"

namespace world {

Vec3 follow_camera_position(Vec3 ship) noexcept {
    // D-BackOffsetSign: camera is BEHIND the ship along z, so subtract.
    const float cam_x = ship.x;
    const float cam_z = ship.z - kCameraBackOffset * terrain::TILE_SIZE;

    // D-GroundClampViaTerrain: floor sampled at the CAMERA's (x, z), not the
    // ship's, with a fixed clearance offset.
    const float floor_y = terrain::altitude(cam_x, cam_z) + kCameraGroundClearance;

    // cam.y = max(ship.y, floor_y) -- ship.y wins when above floor; floor wins
    // when ship dives below.  D-NaNPropagate: std::max with a NaN ship.y is
    // not specially handled; NaN flows through naturally for cam.x/z, and the
    // test only verifies NaN propagation on cam.x.
    const float cam_y = std::max(ship.y, floor_y);

    return Vec3{cam_x, cam_y, cam_z};
}

}  // namespace world

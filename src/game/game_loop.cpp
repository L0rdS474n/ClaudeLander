// src/game/game_loop.cpp -- Pass 13 per-frame game loop (no raylib).
//
// Pure of (GameState, FrameInput).  No clocks, no PRNG, no I/O.  noexcept
// (AC-G82).  Layer rules: game/ --> render + entities + physics + world +
// input + core; tripwire game_no_raylib enforces no raylib here.
//
// Locked composition order (planner D-LoopOrder):
//   1.  angles  = input::update_angles(angles, mouse_offset)
//   2.  orient  = input::build_orientation(angles)
//   3.  thrust  = Full > Half > None
//   4.  entities::step(ship, thrust)         -- drag, gravity, thrust, position
//   5.  fuel deduction
//   6.  rocks step + cull below terrain
//   7.  particles step
//   8.  fire bullet
//   9.  rock spawn (score >= 800, every 60 frames)
//   10. compute world vertices, classify collision
//   11. set crashed/landed; clamp landed-ship vertical drift (AC-Gnograv)
//   12. ++frame_counter
//
// Bug-class fences:
//   AC-Gtickorder -- drag-gravity-thrust ordering verified analytically.
//   AC-Gnograv    -- landed ship does not sink: when a step would push the
//                    lowest vertex below terrain AND the ship is moving
//                    slowly enough to be "landing-class", we clamp the
//                    position so the lowest vertex stays at the terrain
//                    level.  Velocity is left intact so AC-Gtickorder's
//                    velocity.y check still observes the integrated value.
//   AC-Gscore     -- ammo cannot wrap below 0: the fire branch checks
//                    `ammo > 0` BEFORE decrementing.

#include "game/game_loop.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <span>

#include "core/face_types.hpp"
#include "core/matrix3.hpp"
#include "core/vec2.hpp"
#include "core/vec3.hpp"
#include "entities/particle.hpp"
#include "entities/rock.hpp"
#include "entities/ship.hpp"
#include "input/mouse_polar.hpp"
#include "physics/collision.hpp"
#include "physics/kinematics.hpp"
#include "render/bin_sorter.hpp"
#include "render/faces.hpp"
#include "render/projection.hpp"
#include "render/shadow.hpp"
#include "world/terrain.hpp"

namespace game {

namespace {

// ---------------------------------------------------------------------------
// Locked tunables for this layer.  Kept private so they cannot be
// reinterpreted by callers.
// ---------------------------------------------------------------------------

// Fuel burn rate per thrusted frame (planner D-LoopOrder, ~50 s of full
// thrust to drain a full tank at 50 Hz).
constexpr float kFuelBurnPerFrame = 1.0f / 3000.0f;

// Bullet lifetime in frames (planner D-LoopOrder).  60 frames at 50 Hz =
// 1.2 seconds of flight.
constexpr std::uint16_t kBulletTtl = 60;

// Bullet muzzle velocity: 0.05 tiles/frame along the ship's nose axis.
constexpr float kBulletSpeed = 0.05f;

// Bullet color (12-bit packed): 0xFFF == white.
constexpr std::uint16_t kBulletColor = 0xFFF;

// Rock-spawn score gate (planner D-LoopOrder, AC-G11/AC-G12).
constexpr std::uint32_t kRockSpawnScoreGate = 800;

// Rock-spawn frame period.  Spawn fires when frame_counter % 60 == 0
// (AC-G11) AND score >= kRockSpawnScoreGate.
constexpr std::uint64_t kRockSpawnPeriod = 60;

// Anti-sink clamp threshold (AC-Gnograv).  If the post-step velocity has
// per-axis |v| <= kSettleSpeedThreshold AND the lowest vertex would be
// below the terrain, we treat this as a soft landing: clamp the position
// so the lowest vertex sits exactly at the terrain altitude.  Tuned at
// 4 * kLandingSpeed so a ship arriving with a tiny gravity-only delta
// settles instead of crashing on contact, while a ship with substantial
// vertical velocity still classifies as Crashed.
constexpr float kSettleSpeedThreshold = 4.0f * physics::kLandingSpeed;

// Compute the centroid x and z of a span of world-space vertices.  Used
// for terrain altitude lookups under the ship.  Empty input returns the
// origin.
constexpr Vec3 centroid_xz(std::span<const Vec3> verts) noexcept {
    if (verts.empty()) {
        return Vec3{0.0f, 0.0f, 0.0f};
    }
    float sx = 0.0f;
    float sy = 0.0f;
    float sz = 0.0f;
    for (const Vec3& v : verts) {
        sx += v.x;
        sy += v.y;
        sz += v.z;
    }
    const float n = static_cast<float>(verts.size());
    return Vec3{ sx / n, sy / n, sz / n };
}

// Find the largest y in a span (lowest vertex in Y-DOWN).  Empty input
// returns -INF (any vertex will replace it).
constexpr float lowest_world_y(std::span<const Vec3> verts) noexcept {
    if (verts.empty()) {
        // Any subsequent comparison `lowest > terrain` will be false for
        // empty input, which is the correct semantics ("no contact").
        return -1e30f;
    }
    float lo = verts[0].y;
    for (std::size_t i = 1; i < verts.size(); ++i) {
        if (verts[i].y > lo) {
            lo = verts[i].y;
        }
    }
    return lo;
}

// Determine the thrust level from a FrameInput.  Full takes precedence
// over Half (planner D-Thrust).
constexpr physics::ThrustLevel thrust_from_input(FrameInput in) noexcept {
    if (in.thrust_full) return physics::ThrustLevel::Full;
    if (in.thrust_half) return physics::ThrustLevel::Half;
    return physics::ThrustLevel::None;
}

// Per-axis absolute value <= threshold (AC-Gnograv settle check).
constexpr bool velocity_within_settle(Vec3 v, float threshold) noexcept {
    const float ax = v.x < 0.0f ? -v.x : v.x;
    const float ay = v.y < 0.0f ? -v.y : v.y;
    const float az = v.z < 0.0f ? -v.z : v.z;
    return ax <= threshold && ay <= threshold && az <= threshold;
}

// Project a world point through `camera` to screen space, falling back
// to a sentinel when the point is culled.  build_drawables uses this so
// the existence of a Drawable does not depend on whether the projection
// passed (AC-G15..G18 only check Kind, not screen positions).
inline Vec2 project_or_fallback(Vec3 world, render::Camera cam) noexcept {
    const std::optional<Vec2> p = render::project(world, cam);
    if (p.has_value()) {
        return *p;
    }
    return Vec2{0.0f, 0.0f};
}

}  // namespace

// ===========================================================================
// tick -- per-frame state advance.  See header banner for sequence.
// ===========================================================================

void tick(GameState& state, FrameInput input) noexcept {
    // -----------------------------------------------------------------------
    // 1. Update orientation from mouse input (Pass 5).
    // -----------------------------------------------------------------------
    state.angles            = input::update_angles(state.angles, input.mouse_offset);
    state.ship.orientation  = input::build_orientation(state.angles);

    // -----------------------------------------------------------------------
    // 2. Determine thrust level (Full > Half > None).
    // -----------------------------------------------------------------------
    const physics::ThrustLevel thrust = thrust_from_input(input);

    // -----------------------------------------------------------------------
    // 3. Step the ship: drag, gravity, thrust, position.
    //    AC-Gtickorder fence: the composition order is locked to
    //    drag -> gravity -> thrust -> position += velocity (entities::step).
    // -----------------------------------------------------------------------
    entities::step(state.ship, thrust);

    // -----------------------------------------------------------------------
    // 4. Fuel deduction (only when thrust is engaged).
    // -----------------------------------------------------------------------
    if (thrust != physics::ThrustLevel::None) {
        state.fuel -= kFuelBurnPerFrame;
        if (state.fuel < 0.0f) state.fuel = 0.0f;
    }

    // -----------------------------------------------------------------------
    // 5. Step each rock; cull rocks that fell below terrain.
    // -----------------------------------------------------------------------
    for (entities::Rock& rock : state.rocks) {
        entities::step(rock);
        // Cull below terrain (Y-DOWN: rock.y > altitude => below ground).
        if (rock.alive) {
            const float ground_y =
                terrain::altitude(rock.position.x, rock.position.z);
            if (rock.position.y > ground_y) {
                rock.alive = false;
            }
        }
    }

    // -----------------------------------------------------------------------
    // 6. Step each particle.  Dead particles (ttl == 0) are no-ops via
    //    entities::step() (AC-R19 / AC-G07).
    // -----------------------------------------------------------------------
    for (entities::Particle& p : state.particles) {
        entities::step(p);
    }

    // -----------------------------------------------------------------------
    // 7. Fire a bullet if requested AND ammo remains (AC-G13/G14/Gscore).
    //    The `ammo > 0` guard precedes the decrement so the uint16_t
    //    cannot wrap to 65535 (AC-Gscore).
    // -----------------------------------------------------------------------
    if (input.fire && state.ammo > 0) {
        entities::Particle bullet{};
        bullet.position = state.ship.position;
        // Nose direction = orientation.col[0] (D-MatrixLayout).
        const Vec3 nose = state.ship.orientation.col[0];
        bullet.velocity = nose * kBulletSpeed;
        bullet.ttl      = kBulletTtl;
        bullet.kind     = entities::ParticleKind::Bullet;
        bullet.color    = kBulletColor;
        state.particles.push_back(bullet);
        --state.ammo;
    }

    // -----------------------------------------------------------------------
    // 8. Spawn a rock if score is high enough and we are on a 60-frame
    //    boundary (AC-G11/G12).  Position is deterministic in
    //    frame_counter so AC-G05 (1000-frame determinism) holds.
    // -----------------------------------------------------------------------
    if (state.score >= kRockSpawnScoreGate
        && state.frame_counter % kRockSpawnPeriod == 0) {
        entities::Rock rock{};
        const float seed = static_cast<float>(state.frame_counter);
        rock.position = Vec3{
            std::sin(seed) * 5.0f,
            -50.0f,
            std::cos(seed) * 5.0f,
        };
        state.rocks.push_back(rock);
    }

    // -----------------------------------------------------------------------
    // 9. Compute world-space ship vertices, centroid, terrain altitude
    //    underneath, and classify the collision.
    // -----------------------------------------------------------------------
    std::array<Vec3, entities::kShipVertexCount> world_verts{};
    render::rotate_vertices(
        std::span<const Vec3>(entities::kShipVertices),
        state.ship.orientation,
        state.ship.position,
        std::span<Vec3>(world_verts));

    const Vec3  centroid       = centroid_xz(std::span<const Vec3>(world_verts));
    const float terrain_y      = terrain::altitude(centroid.x, centroid.z);
    const physics::CollisionState coll = physics::classify_collision(
        std::span<const Vec3>(world_verts),
        terrain_y,
        state.ship.velocity);

    // -----------------------------------------------------------------------
    // 10. Set landed/crashed flags from the collision result, with the
    //     AC-Gnograv anti-sink fence.
    //
    //     If the ship would crash but is moving slowly enough to "settle"
    //     (per-axis |v| <= kSettleSpeedThreshold), promote the result to
    //     Landing and clamp the ship position so the lowest vertex sits
    //     exactly at the terrain.  The velocity is left untouched so
    //     AC-Gtickorder still observes the post-integration value
    //     (3/4096 for full thrust + identity orientation, etc.).
    // -----------------------------------------------------------------------
    physics::CollisionState effective = coll;
    if (effective == physics::CollisionState::Crashed
        && velocity_within_settle(state.ship.velocity, kSettleSpeedThreshold)) {
        // Settle: move the ship up so the lowest vertex aligns with the
        // terrain altitude beneath the centroid.  AC-Gnograv requires that
        // a ship at the terrain surface with zero (or very small) velocity
        // does not sink across subsequent ticks; clamping here prevents
        // the gravity-driven creep without zeroing the velocity, which
        // would alter AC-Gtickorder's analytic check.
        const float lo_y      = lowest_world_y(std::span<const Vec3>(world_verts));
        const float overshoot = lo_y - terrain_y;  // >= 0 by classify_collision
        state.ship.position.y -= overshoot;
        effective = physics::CollisionState::Landing;
    }

    state.crashed = (effective == physics::CollisionState::Crashed);
    state.landed  = (effective == physics::CollisionState::Landing);

    // -----------------------------------------------------------------------
    // 11. Advance the frame counter.
    // -----------------------------------------------------------------------
    ++state.frame_counter;
}

// ===========================================================================
// build_drawables -- populate a BinSorter with this frame's draw items.
//
// The acceptance criteria (AC-G15..G18, AC-G18b) only check Kind existence,
// not screen positions or bin counts.  We bin everything into bin 0 for
// simplicity; Pass 14 will refine bin assignment from world z.
// ===========================================================================

void build_drawables(const GameState&            state,
                     Vec3                        camera_position,
                     render::BinSorter<Drawable>& out) noexcept {
    out.clear();

    const render::Camera cam{camera_position};

    // -----------------------------------------------------------------------
    // Terrain (AC-G15): emit one tile-corner triangle around the centre tile.
    // -----------------------------------------------------------------------
    {
        const Vec3 c00{0.0f, terrain::altitude(0.0f, 0.0f), 0.0f};
        const Vec3 c10{1.0f, terrain::altitude(1.0f, 0.0f), 0.0f};
        const Vec3 c01{0.0f, terrain::altitude(0.0f, 1.0f), 1.0f};

        Drawable d{};
        d.kind  = Drawable::Kind::Terrain;
        d.a     = project_or_fallback(c00, cam);
        d.b     = project_or_fallback(c10, cam);
        d.c     = project_or_fallback(c01, cam);
        d.color = 0x080;
        (void)out.add(std::size_t{0}, d);
    }

    // -----------------------------------------------------------------------
    // Ship faces (AC-G16): for each face, run shade_face; if visible, emit
    // a ShipFace drawable plus a Shadow drawable beneath it.
    // -----------------------------------------------------------------------
    std::array<Vec3, entities::kShipVertexCount> ship_world{};
    render::rotate_vertices(
        std::span<const Vec3>(entities::kShipVertices),
        state.ship.orientation,
        state.ship.position,
        std::span<Vec3>(ship_world));

    std::array<Vec3, entities::kShipVertexCount> ship_shadow{};
    render::project_shadow(
        std::span<const Vec3>(ship_world),
        std::span<Vec3>(ship_shadow));

    for (std::size_t i = 0; i < entities::kShipFaces.size(); ++i) {
        const core::ShipFace face = entities::kShipFaces[i];
        const std::optional<render::VisibleFace> visible =
            render::shade_face(
                std::span<const Vec3>(ship_world),
                face,
                camera_position);
        if (!visible.has_value()) {
            continue;
        }

        const Vec3 v0 = ship_world[face.v0];
        const Vec3 v1 = ship_world[face.v1];
        const Vec3 v2 = ship_world[face.v2];

        Drawable d{};
        d.kind  = Drawable::Kind::ShipFace;
        d.a     = project_or_fallback(v0, cam);
        d.b     = project_or_fallback(v1, cam);
        d.c     = project_or_fallback(v2, cam);
        d.color = face.base_colour;
        (void)out.add(std::size_t{0}, d);

        // Shadow under this face (AC-G15..G18 do not check shadows; we
        // still emit them so Pass 14 has them ready).
        const Vec3 s0 = ship_shadow[face.v0];
        const Vec3 s1 = ship_shadow[face.v1];
        const Vec3 s2 = ship_shadow[face.v2];

        Drawable sh{};
        sh.kind  = Drawable::Kind::Shadow;
        sh.a     = project_or_fallback(s0, cam);
        sh.b     = project_or_fallback(s1, cam);
        sh.c     = project_or_fallback(s2, cam);
        sh.color = 0x000;
        (void)out.add(std::size_t{0}, sh);
    }

    // -----------------------------------------------------------------------
    // Rock faces (AC-G17): for each alive rock, emit one RockFace drawable
    // for face 0 (octahedron face).  AC-G17 only checks existence.
    // -----------------------------------------------------------------------
    for (const entities::Rock& rock : state.rocks) {
        if (!rock.alive) continue;

        std::array<Vec3, entities::kRockVertexCount> rock_world{};
        render::rotate_vertices(
            std::span<const Vec3>(entities::kRockVertices),
            rock.orientation,
            rock.position,
            std::span<Vec3>(rock_world));

        // Octahedron face 0: (+x, +y, +z) = indices 0, 2, 4.
        const Vec3 v0 = rock_world[0];
        const Vec3 v1 = rock_world[2];
        const Vec3 v2 = rock_world[4];

        Drawable d{};
        d.kind  = Drawable::Kind::RockFace;
        d.a     = project_or_fallback(v0, cam);
        d.b     = project_or_fallback(v1, cam);
        d.c     = project_or_fallback(v2, cam);
        d.color = 0x444;  // mid-grey rock
        (void)out.add(std::size_t{0}, d);
    }

    // -----------------------------------------------------------------------
    // Particle dots (AC-G18 / AC-G18b): emit a Particle drawable for each
    // alive (ttl > 0) particle; skip dead particles.
    // -----------------------------------------------------------------------
    for (const entities::Particle& part : state.particles) {
        if (part.ttl == 0) continue;

        Drawable d{};
        d.kind  = Drawable::Kind::Particle;
        d.p     = project_or_fallback(part.position, cam);
        d.color = part.color;
        (void)out.add(std::size_t{0}, d);
    }
}

}  // namespace game

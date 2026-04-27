// src/world/object_map.cpp -- Pass 11 deterministic ground-object placement
// implementation.
//
// Pure-hash placement over a 256x256 wrapped tile grid (planner
// D-PlacementHash, D-MapSize, D-Density, D-Stateless, D-NoLut):
//
//   wx = ((tx % 256) + 256) % 256        -- positive-mod wrap
//   wz = ((tz % 256) + 256) % 256
//   if (wx == 0 && wz == 0) -> nullopt   -- launchpad carve-out (AC-Olaunchpad)
//   hash = (uint32_t(wx) * 73856093u) ^ (uint32_t(wz) * 19349663u)
//   if (hash % 100 >= 30)   -> nullopt   -- ~70% empty
//   type_idx = (hash >> 8) % 5 + 1       -- 1..5 -> Tree..FirTree (skip None=0)
//   position = {wx*TILE_SIZE, altitude(x,z), wz*TILE_SIZE}
//
// hit_by_bullet performs a 3x3 tile-neighbourhood scan around the bullet's
// floor-tile centre with an INCLUSIVE 0.5-tile radius (planner AC-O18: the
// boundary at exactly 0.5 tiles is a hit).
//
// Boundary: includes core/ + entities/ + world/terrain only.  No raylib, no
// render/, no <random>, no <chrono>.  Enforced by the world_no_raylib_includes
// CTest tripwire (CMakeLists.txt) and the AC-O80 static_assert inside
// tests/test_object_map.cpp.

#include "world/object_map.hpp"

#include <cmath>
#include <cstdint>

#include "core/vec3.hpp"
#include "entities/ground_object.hpp"
#include "world/terrain.hpp"

namespace world {

namespace {

// Hash a (tx, tz) tile coordinate into a 32-bit value.  Constants are the
// canonical 73856093 / 19349663 pair (planner D-PlacementHash); they are
// odd primes chosen to spread bits well under XOR.  The cast through
// uint32_t makes the bit pattern of negative ints well-defined under the
// modular arithmetic of unsigned multiplication.
constexpr std::uint32_t hash_tile(int tx, int tz) noexcept {
    const auto utx = static_cast<std::uint32_t>(tx);
    const auto utz = static_cast<std::uint32_t>(tz);
    return (utx * 73856093u) ^ (utz * 19349663u);
}

// Positive-mod wrap into [0, kObjectMapSize).  C++ `%` can return a negative
// remainder for negative operands, so the canonical
//   ((t % s) + s) % s
// idiom is used to guarantee a non-negative result.  AC-O08 verifies that
// object_at(0,0) == object_at(256,0) -- both wrap to (0,0) and hit the
// launchpad carve-out.
constexpr int wrap_tile(int t) noexcept {
    constexpr int s = kObjectMapSize;
    return ((t % s) + s) % s;
}

// Density threshold (planner D-Density).  hash % 100 < 30 -> populated, so
// ~30% of tiles host an object across the 65536-tile grid (AC-O10, AC-Odensity).
constexpr std::uint32_t kDensityThreshold = 30u;

// Number of concrete object kinds (Tree..FirTree); does not count None.
// Used for type assignment from (hash >> 8) % kObjectKindCount + 1.
constexpr std::uint32_t kObjectKindCount = 5u;

}  // namespace

std::optional<entities::GroundObject> object_at(int tx, int tz) noexcept {
    // Step 1: wrap into [0, 256) so out-of-range inputs (including negatives
    // produced by hit_by_bullet's 3x3 neighbourhood scan) map onto the same
    // canonical tile as their in-range siblings.
    const int wx = wrap_tile(tx);
    const int wz = wrap_tile(tz);

    // Step 2: launchpad carve-out (AC-Olaunchpad bug-class fence).  Applied
    // AFTER wrapping so anything that wraps to (0,0) is also empty -- this is
    // what makes AC-O08 (object_at(256,0) == object_at(0,0)) hold by
    // construction rather than by hash coincidence.
    if (wx == 0 && wz == 0) {
        return std::nullopt;
    }

    // Step 3: hash + density gate.  hash % 100 in [0, 29] -> populated;
    // [30, 99] -> empty.  ~30% of tiles populated overall (AC-O10).
    const std::uint32_t h = hash_tile(wx, wz);
    if ((h % 100u) >= kDensityThreshold) {
        return std::nullopt;
    }

    // Step 4: type assignment.  (h >> 8) % 5 + 1 yields a value in [1, 5];
    // the +1 skips None=0 so populated tiles are never the None sentinel.
    // (h >> 8) is used instead of h directly so the density bits (low 7 bits
    // dominate hash % 100) and the type bits are statistically independent.
    const auto type_idx =
        static_cast<std::uint8_t>(((h >> 8) % kObjectKindCount) + 1u);

    // Step 5: world-space position.  x and z are the wrapped tile coordinates
    // scaled by TILE_SIZE; y is altitude(x, z) so the object sits on terrain
    // (AC-O13, AC-O20).  altitude is a pure function of (x, z) (Pass 2
    // contract) -- no PRNG state escapes here.
    const float fx = static_cast<float>(wx) * terrain::TILE_SIZE;
    const float fz = static_cast<float>(wz) * terrain::TILE_SIZE;
    const float fy = terrain::altitude(fx, fz);

    return entities::GroundObject{
        Vec3{ fx, fy, fz },
        static_cast<entities::ObjectType>(type_idx),
        true,
    };
}

bool hit_by_bullet(Vec3 b) noexcept {
    // The bullet's tile centre is the floor-rounded division by TILE_SIZE.
    // std::floor handles negative b.x / b.z correctly (rounds toward -inf),
    // matching wrap_tile's positive-mod semantics in object_at.
    const int center_tx =
        static_cast<int>(std::floor(b.x / terrain::TILE_SIZE));
    const int center_tz =
        static_cast<int>(std::floor(b.z / terrain::TILE_SIZE));

    // 3x3 neighbourhood: any object within 0.5 tiles of the bullet must live
    // in the bullet's own tile or one of the eight adjacent tiles.  Testing
    // the 3x3 window is sufficient because the per-tile object position is
    // anchored at the tile corner and the 0.5-tile radius is half a tile.
    constexpr float kRadius = 0.5f * terrain::TILE_SIZE;
    constexpr float kRadiusSq = kRadius * kRadius;

    for (int dz = -1; dz <= 1; ++dz) {
        for (int dx = -1; dx <= 1; ++dx) {
            const auto obj = object_at(center_tx + dx, center_tz + dz);
            if (!obj.has_value()) continue;

            const Vec3 d = b - obj->position;
            const float dist_sq = d.x * d.x + d.y * d.y + d.z * d.z;

            // INCLUSIVE boundary (planner AC-O18: "true at exact 0.5").  Use
            // squared distance with `<=` to avoid a square-root and to keep
            // the boundary case bit-exact.
            if (dist_sq <= kRadiusSq) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace world

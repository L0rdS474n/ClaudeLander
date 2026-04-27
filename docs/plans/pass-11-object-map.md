# Pass 11 Object Map + Ground Objects — Planner Output (Gate 1)

**Branch:** `feature/pass-11-object-map`
**Date:** 2026-04-27
**Baseline:** 299/299 tests green (Pass 1..10).

## 1. Problem restatement

Two new modules:

1. `src/world/object_map.{hpp,cpp}` — deterministic placement of ground
   objects on a 256×256 tile grid. Each tile `(tx, tz)` maps to either
   `std::nullopt` (empty) or a `GroundObject`. Hash-based placement,
   no PRNG state.

2. `src/entities/ground_object.{hpp,cpp}` — object types (Tree,
   Building, Gazebo, Rocket, FirTree), score-on-hit table, alive flag.

Pass 11 adds bullet-vs-object collision (`bool hit_by_bullet(...)`) and
ship-vs-object collision sketch (deferred per-axis to Pass 13 wiring).

## 2. Locked design decisions

| ID | Decision |
|----|----------|
| **D-MapSize** | 256 × 256 tiles. Deterministic placement via hash. |
| **D-PlacementHash** | `hash(tx, tz) = (tx * 73856093) ^ (tz * 19349663)` (Pass 1 PRNG-equivalent). Result mod density determines empty vs object. |
| **D-Density** | ~30% of tiles have an object. Hash-stable. |
| **D-ObjectTypes** | Tree, Building, Gazebo, Rocket, FirTree (5 types). |
| **D-ScoreTable** | Tree=10, FirTree=10, Gazebo=20, Building=50, Rocket=100. |
| **D-LaunchpadCarveout** | Tile (0,0) is forced empty (launchpad). |
| **D-Stateless** | All functions noexcept, pure. |
| **D-NoLut** | No tables; computed per call. |

## 3. Public API

```cpp
namespace entities {
    enum class ObjectType : std::uint8_t {
        None = 0, Tree, Building, Gazebo, Rocket, FirTree,
    };

    struct GroundObject {
        Vec3 position;
        ObjectType type = ObjectType::None;
        bool alive = true;
    };

    int score_for(ObjectType t) noexcept;  // returns 0 for None
}

namespace world {
    inline constexpr int kObjectMapSize = 256;

    // Compute the deterministic object at a given tile.
    // Returns nullopt if empty.
    std::optional<entities::GroundObject> object_at(int tx, int tz) noexcept;

    // Bullet hit test: returns true if (bx, by, bz) is within
    // a ±0.5 tile radius of an object's position.
    bool hit_by_bullet(Vec3 bullet_position) noexcept;
}
```

## 4. Acceptance criteria (24)

### Score table (AC-O01..O05)
- AC-O01 — `score_for(Tree) == 10`.
- AC-O02 — `score_for(FirTree) == 10`.
- AC-O03 — `score_for(Gazebo) == 20`.
- AC-O04 — `score_for(Building) == 50`.
- AC-O05 — `score_for(Rocket) == 100`. `score_for(None) == 0`.

### object_at determinism (AC-O06..O10)
- AC-O06 — `object_at(0, 0) == nullopt` (launchpad).
- AC-O07 — `object_at(tx, tz)` is deterministic; same inputs → same output.
- AC-O08 — Map repeats every 256 tiles: `object_at(0, 0) == object_at(256, 0)`.
- AC-O09 — A non-empty tile returns a populated GroundObject.
- AC-O10 — Total density: at least 25% and at most 35% of 256×256 tiles populated.

### Position semantics (AC-O11..O14)
- AC-O11 — `object_at(5, 7).position.x ≈ 5 * TILE_SIZE`.
- AC-O12 — `object_at(5, 7).position.z ≈ 7 * TILE_SIZE`.
- AC-O13 — `object_at(...).position.y ≈ altitude(position.x, position.z)` (sits on terrain).
- AC-O14 — Type assignment is deterministic per (tx, tz).

### Bullet collision (AC-O15..O18)
- AC-O15 — Bullet at (5*TILE_SIZE, altitude, 7*TILE_SIZE) where object exists → `hit_by_bullet == true`.
- AC-O16 — Bullet far from any object → `hit_by_bullet == false`.
- AC-O17 — Bullet at launchpad (0,0,0) → false (no object there).
- AC-O18 — Bullet 0.5 tile from object → boundary case; planner default = true at exact 0.5.

### Determinism + integration (AC-O19..O20)
- AC-O19 — `object_at` 1000 calls bit-identical across runs.
- AC-O20 — All object positions sit on terrain altitude (within `kObjEps`).

### Bug-class fences
- **AC-Olaunchpad** — tile (0,0) MUST be empty (or game-start-crash impossible).
- **AC-Odensity** — density in `[25%, 35%]` (rejects 0% empty map and 100% impassable).
- **AC-Oscore** — Rocket > Building > Gazebo > Tree=FirTree (score order pinned).

### Hygiene (AC-O80..O82)
- AC-O80 — `src/world/object_map.{hpp,cpp}` and `src/entities/ground_object.{hpp,cpp}` exclude raylib/render/`<random>`/`<chrono>`.
- AC-O81 — `claude_lander_world` link list unchanged; `claude_lander_entities` link list unchanged.
- AC-O82 — All public functions `noexcept`.

## 5. Test plan

`tests/test_object_map.cpp` — AC-O01..O20 + fences + hygiene. Tag `[world][object_map]`.
Tolerance: `kObjEps = 1e-4f`.

## 6. CMake plan

- Append `src/world/object_map.cpp` to `claude_lander_world`.
- Append `src/entities/ground_object.cpp` to `claude_lander_entities`.
- Append test to test sources.

## 7. Definition of Done

- [ ] All 24 ACs green.
- [ ] 299-baseline preserved.
- [ ] No forbidden includes; no PRNG state (uses pure hash).
- [ ] PR body: `Closes #TBD-pass-11` + DEFERRED + WIRING.

## 8. ADR triggers

Zero. Same patterns as Pass 2/4/10.

## 9. Open questions

- KOQ-1: empirical density target → Pass 14.
- KOQ-2: object mesh definitions (vertices per object type) → Pass 13.
- KOQ-3: ship-vs-object collision → Pass 13 wires.
- KOQ-4: object respawn on score reset → Pass 13.

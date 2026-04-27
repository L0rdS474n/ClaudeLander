# Pass 8 Bin Sorter — Mathematical Specification

> Gate 1.5 research output for `feature/pass-8-bin-sorter`.
> Prose-only behavioural spec from <https://lander.bbcelite.com>.

## Sources read

- <https://lander.bbcelite.com/deep_dives/depth-sorting_with_the_graphics_buffers.html>
- <https://lander.bbcelite.com/source/main/variable/graphicsbuffers.html>
- <https://lander.bbcelite.com/deep_dives/drawing_3d_objects.html>
- <https://lander.bbcelite.com/deep_dives/drawing_the_landscape.html>

## 1. Buffer structure

12 buffers total: 11 active (bin 0..10), 1 unused/reserved (bin 11).

- bin 0  — furthest tile row (horizon)
- bin 10 — nearest tile row (closest to camera)

Each tile-row in the 13×11 landscape mesh corresponds to one bin (10
drawable rows → 11 corner rows → 11 bins).

## 2. Bin assignment

```
bin = LANDSCAPE_Z - z_coordinate
```

- `LANDSCAPE_Z` is the world-space z of the furthest tile row.
- `z_coordinate` is the drawable's representative depth (centroid for
  faces; pivot for particles).
- The result is an integer in `[0, 10]` (or 11 if we accept the unused
  slot).

Out-of-range drawables: planner default is to **discard** silently
(out of camera frustum).

## 3. Drawables assigned to bins

Every drawable that participates in painter's-algorithm depth ordering:

- Terrain tile triangles (Pass 2/8 wiring).
- Object faces (ship, rocks, ground objects).
- Particles (exhaust, explosions, bullets, sparks).
- Shadows.

In bbcelite, each drawable is encoded as a "command + data" record
into the buffer. For the C++ port we abstract this — the bin holds a
type-erased payload (or a `std::variant`) and the renderer dispatches
on type when iterating.

## 4. Within-bin ordering

**Insertion order**, no within-bin sort.

Originally rationalised by the bbcelite design as a way to skip a
per-frame quicksort: the ARM hardware was too slow for that. The
practical consequence is that drawables in the same tile row may
overdraw each other unpredictably; this is acceptable for the
isometric bird's-eye Lander view because in-row overlaps are
visually rare.

For the C++ port we preserve insertion order to match the original
behaviour. If a future pass shows visible glitches, Pass 14 may add
within-bin depth sort behind a flag.

## 5. Across-bin ordering

Render bins **0 → 10** (far to near) — painter's algorithm at tile-row
granularity.

Far bins drawn first, then progressively closer rows overwrite them.
Result: closer items occlude farther items as expected.

## 6. Two-row stagger (informative)

The original interleaves landscape rows and object/particle rows at a
two-row delay so terrain renders behind its overlying objects. For the
C++ port this happens naturally if every drawable (terrain, object,
particle, shadow) is added to the same `BinSorter` and then the renderer
iterates 0..10 once. The delay was a memory-layout optimisation, not
a depth-correctness requirement, and is OUT OF SCOPE for Pass 8.

## 7. Buffer terminator (informative)

Bbcelite uses a per-buffer terminator (command 19 or zero word) to mark
end-of-data. The C++ port replaces this with `std::vector<Drawable>`
sized at runtime — no terminator needed.

## 8. Public API recommendation

```cpp
namespace render {
    inline constexpr std::size_t kBinCount = 11;

    template <typename Payload>
    class BinSorter {
        std::array<std::vector<Payload>, kBinCount> bins_;
    public:
        void clear() noexcept;                        // empties every bin
        bool add(int bin_index, Payload p);           // false if out of range
        bool add_by_z(float landscape_z, float z, Payload p);
        std::span<const Payload> bin(std::size_t i) const noexcept;

        // Visit drawables in painter order (bin 0 first → bin 10 last);
        // within a bin, insertion order.
        template <typename F> void for_each_back_to_front(F&& visit) const;
    };
}
```

## 9. Determinism

Pure data structure. No PRNG, no clock. Same insertion sequence →
identical iteration sequence, bit-identically.

## 10. Open questions / DEFERRED

1. **Within-bin sort opt-in** — Pass 14 may revisit if visible glitches.
2. **`LANDSCAPE_Z` numerical value** — depends on Pass 7 camera offset;
   planner default uses ship.z + 5 (the back-offset) as the "near" edge
   of the visible terrain frustum. Pass 13 wires this.
3. **Payload type** — Pass 13 will define a concrete `Drawable` variant
   (face, triangle, particle, shadow). Pass 8 only ships the templated
   container.
4. **Out-of-range policy** — discard vs clamp. Planner default: discard
   (return false from `add_by_z`).

## 11. Clean-room boundary

Behaviour described in prose only. No ARM hex constants beyond
`LANDSCAPE_Z` cited as a name (its float value is a planner choice).
Buffer-encoding hex (commands 0..18) is mentioned in prose but the
C++ port replaces the encoding with a typed payload, so no hex is
transcribed.

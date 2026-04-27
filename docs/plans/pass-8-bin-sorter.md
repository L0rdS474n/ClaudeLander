# Pass 8 Bin Sorter — Planner Output (Gate 1)

**Branch:** `feature/pass-8-bin-sorter`
**Date:** 2026-04-27
**Baseline:** 217/217 tests green (Pass 1..7).

## 1. Problem restatement

Implement Lander's bin-sort depth-ordering: 11 bins, one per tile row,
drawables assigned via `bin = LANDSCAPE_Z - z`. Bins iterated 0→10
(far→near) — painter's algorithm at tile-row granularity, no per-frame
sort.

This pass ships the **data structure only**; concrete `Drawable`
variant and renderer wiring land in Pass 13.

**Module placement:** `src/render/bin_sorter.{hpp,cpp}` extends
`claude_lander_render`. Templated on payload, so most of the code lives
in the header.

## 2. Locked design decisions

| ID | Decision |
|----|----------|
| **D-BinCount** | `kBinCount = 11`. |
| **D-PainterOrder** | Iterate 0 → kBinCount-1 = far → near. |
| **D-InsertionOrder** | No within-bin sort; payload preserves push order. |
| **D-Templated** | `BinSorter<Payload>` — Pass 13 binds `Payload`. |
| **D-DiscardOOR** | Out-of-range bin returns false from `add_by_z`. |
| **D-Stateless API** | Construction is trivial; `clear()` resets all bins. |
| **D-Deterministic** | Same sequence in → same sequence out, bit-identical. |
| **D-NoSortOnRead** | Iteration is read-only; no lazy sort. |

## 3. Public API

```cpp
namespace render {
    inline constexpr std::size_t kBinCount = 11;

    template <typename Payload>
    class BinSorter {
        std::array<std::vector<Payload>, kBinCount> bins_;
    public:
        void clear() noexcept;
        bool add(std::size_t bin_index, Payload payload);
        bool add_by_z(float landscape_z, float z, Payload payload);
        std::span<const Payload> bin(std::size_t i) const noexcept;
        std::size_t size(std::size_t i) const noexcept;
        std::size_t total_size() const noexcept;
        template <typename F> void for_each_back_to_front(F&& visit) const;
    };

    // Helper: clamp a float landscape_z - z to a bin index.
    // Returns std::nullopt when out of range.
    std::optional<std::size_t> bin_for_z(float landscape_z, float z) noexcept;
}
```

`add_by_z` computes `bin = static_cast<std::size_t>(landscape_z - z)`
and forwards to `add(bin, ...)`. Out of `[0, kBinCount)` → returns false.

## 4. Acceptance criteria (24)

### Bin assignment (AC-B01..B05)
- AC-B01 — `bin_for_z(10.0f, 10.0f) == 0` (z exactly at landscape_z).
- AC-B02 — `bin_for_z(10.0f, 0.0f) == 10` (z = 0, furthest practical).
- AC-B03 — `bin_for_z(10.0f, 11.0f) == nullopt` (negative bin).
- AC-B04 — `bin_for_z(10.0f, -1.0f) == nullopt` (bin 11 ≥ kBinCount).
- AC-B05 — `bin_for_z` deterministic; bit-identical across two runs.

### add / add_by_z (AC-B06..B10)
- AC-B06 — Default-constructed `BinSorter<int>` has `total_size() == 0` and `bin(i).empty()` for all i.
- AC-B07 — `add(0, 42)` → `bin(0).size() == 1` and `bin(0)[0] == 42`.
- AC-B08 — `add` to out-of-range bin returns false; sizes unchanged.
- AC-B09 — `add_by_z(10.0f, 10.0f, 99)` → `bin(0)[0] == 99`.
- AC-B10 — `add_by_z(10.0f, 100.0f, 0)` returns false (out of range); sizes unchanged.

### Insertion order (AC-B11..B14)
- AC-B11 — Adding `1, 2, 3` to bin 0 yields `bin(0) == {1, 2, 3}` exactly.
- AC-B12 — Adding to multiple bins preserves per-bin order.
- AC-B13 — `for_each_back_to_front` walks bin 0 first, bin 10 last.
- AC-B14 — Within `for_each_back_to_front`, items in bin i appear in insertion order.

### clear + reuse (AC-B15..B17)
- AC-B15 — After `clear()`, all bins empty; `total_size() == 0`.
- AC-B16 — Reuse after `clear()` works as if fresh-constructed.
- AC-B17 — `clear()` does not reduce capacity (perf invariant; non-binding).

### Determinism (AC-B18..B20)
- AC-B18 — Same insertion sequence → identical iteration sequence (1000 items).
- AC-B19 — `total_size()` matches sum of `size(i)` for all i.
- AC-B20 — Iteration order is stable across two clear-and-refill cycles.

### Bug-class fences (AC-Bfar/AC-Bnear/AC-Boor)
- **AC-Bfar** — Bin 0 corresponds to FAR; iteration starts there. A reversed-iteration bug fails AC-B13.
- **AC-Bnear** — Bin 10 corresponds to NEAR; iteration ends there.
- **AC-Boor** — Out-of-range adds are rejected, NOT clamped to bin 0 or 10.

### Hygiene (AC-B80..B82)
- AC-B80 — `src/render/bin_sorter.{hpp,cpp}` excludes raylib/world/entities/physics/`<random>`/`<chrono>`. Existing `render` tripwire covers.
- AC-B81 — `claude_lander_render` link list unchanged.
- AC-B82 — `static_assert(noexcept(BinSorter<int>{}.clear()))` and `noexcept(bin_for_z(0.0f, 0.0f))`.

## 5. Test plan

`tests/test_bin_sorter.cpp`. Tag `[render][bin_sorter]`. Tolerance unused
(integer payloads in tests). No `<random>`, no `<chrono>`.

## 6. CMake plan

Append `src/render/bin_sorter.cpp` to `claude_lander_render`. Append
`tests/test_bin_sorter.cpp` to test sources. No new tripwires.

## 7. Definition of Done

- [ ] All 24 ACs green.
- [ ] 217-test baseline preserved.
- [ ] No forbidden includes.
- [ ] `noexcept` static_asserts present.
- [ ] PR body: `Closes #TBD-pass-8` + DEFERRED + WIRING.

## 8. ADR triggers

Zero. Templated container, no new dependencies.

## 9. Open questions

- KOQ-1: within-bin sort opt-in → Pass 14.
- KOQ-2: concrete Drawable variant → Pass 13.
- KOQ-3: LANDSCAPE_Z numerical value → Pass 13 wires from camera/terrain.
- KOQ-4: capacity hints / arena allocator → Pass 14 if profiling demands.

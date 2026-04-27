// src/render/bin_sorter.hpp -- Pass 8 bin-sort depth ordering (clean-room).
//
// =============================================================================
//  PAINTER ORDER (D-PainterOrder, locked) -- READ BEFORE EDITING
// =============================================================================
//
//   for_each_back_to_front iterates bin 0 FIRST (far / horizon) through
//   bin 10 LAST (near).  This is painter's algorithm at tile-row granularity:
//   far objects are drawn first and overwritten by progressively nearer ones.
//
//   Reversing the iteration direction (bin 10 first -> bin 0 last) silently
//   inverts depth: near objects are drawn UNDER far objects.  AC-Bfar / AC-Bnear
//   are the loud guard tests for this exact bug class.  If a refactor seems to
//   require iterating in reverse, STOP and re-read this banner and the spec.
//
// =============================================================================
//  BIN ASSIGNMENT (D-BinCount, locked)
// =============================================================================
//
//   bin = LANDSCAPE_Z - z   (integer truncation of the float difference)
//
//   * kBinCount = 11.  Valid bins are 0..10 inclusive.
//   * bin 0  = furthest tile row (z == LANDSCAPE_Z).
//   * bin 10 = nearest tile row.
//   * Out-of-range drawables are REJECTED (D-DiscardOOR), NOT clamped.
//
//   AC-Boor is the loud guard test: silently clamping to bin 0 or bin 10
//   would put off-frustum geometry into the visible scene.  Do NOT clamp.
//
// =============================================================================
//
// Boundaries:
//   render/ depends on core/ ONLY.  No raylib, no world/, no entities/,
//   no <random>, no <chrono>.  The render_no_raylib_or_world_includes CTest
//   tripwire and the static_assert(!defined(RAYLIB_VERSION)) in the test TU
//   enforce this; AC-B80 backs it at compile time.
//
// AC-B82: BinSorter<P>::clear() and bin_for_z are noexcept.  The test TU
// pins this with static_asserts at the top of the file.
//
// Determinism (D-Deterministic): pure data structure, no PRNG, no clock,
// no global mutable state.  Same insertion sequence -> bit-identical
// iteration sequence.  AC-B05 / AC-B18 / AC-B20 verify this.
//
// References:
//   docs/plans/pass-8-bin-sorter.md          (D-* decisions, AC list)
//   docs/research/pass-8-bin-sorter-spec.md  (mathematical spec)

#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace render {

// ---------------------------------------------------------------------------
// kBinCount -- D-BinCount: 11 active bins (0..10).
// ---------------------------------------------------------------------------
//
// Bin 0 corresponds to the furthest tile row at z == LANDSCAPE_Z.
// Bin 10 corresponds to the nearest tile row at z == LANDSCAPE_Z - 10.
// Indices >= kBinCount are out of range and are rejected by add() and
// add_by_z() per D-DiscardOOR.

inline constexpr std::size_t kBinCount = 11;

// ---------------------------------------------------------------------------
// bin_for_z -- map a (landscape_z, z) pair to a bin index, or std::nullopt.
// ---------------------------------------------------------------------------
//
//   delta = landscape_z - z
//   if delta < 0 or delta >= kBinCount  -> std::nullopt
//   else                                -> static_cast<std::size_t>(delta)
//
// Truncation toward zero (the integer-cast semantics) maps a float in
// [n, n+1) to bin n, matching the original ARM integer subtraction.
//
// Pure: no PRNG, no clock, noexcept (AC-B82).

std::optional<std::size_t> bin_for_z(float landscape_z, float z) noexcept;

// ---------------------------------------------------------------------------
// BinSorter<Payload> -- 11-bin painter-order container.
// ---------------------------------------------------------------------------
//
// Templated on Payload so that Pass 13 can bind a concrete Drawable variant
// without re-coding the container.  Pass 8 ships the data structure only.
//
// Storage:
//   std::array<std::vector<Payload>, kBinCount>
//
// Insertion:
//   add(bin_index, payload)         -- direct index, returns false if OOR.
//   add_by_z(landscape_z, z, payload) -- delegates to bin_for_z + add.
//
// Iteration:
//   for_each_back_to_front(visit)   -- bin 0 -> bin kBinCount-1, in
//                                      insertion order within each bin.
//                                      visit is invoked as visit(payload).
//
// Reset:
//   clear() -- empties every bin.  Does NOT reduce capacity (AC-B17).
//
// Determinism: no PRNG, no clock, no global state.  Identical insertion
// sequence -> identical iteration sequence (AC-B18, AC-B20).

template <typename Payload>
class BinSorter {
public:
    // Empty all bins.  Per AC-B17 this preserves capacity; callers that
    // re-fill the same volume after clear() see no allocations.  noexcept
    // because std::vector::clear is noexcept.  AC-B82.
    void clear() noexcept {
        for (auto& bin : bins_) {
            bin.clear();
        }
    }

    // Insert payload into bin_index.  Returns false (and leaves all bins
    // unchanged) when bin_index >= kBinCount.  D-DiscardOOR / AC-B08 /
    // AC-Boor: do NOT clamp out-of-range indices.
    bool add(std::size_t bin_index, Payload payload) {
        if (bin_index >= kBinCount) {
            return false;
        }
        bins_[bin_index].push_back(std::move(payload));
        return true;
    }

    // Insert payload at the bin computed from (landscape_z, z).  Returns
    // false (and leaves all bins unchanged) when the resulting bin index
    // is out of [0, kBinCount).  AC-B09 / AC-B10.
    bool add_by_z(float landscape_z, float z, Payload payload) {
        const auto idx = bin_for_z(landscape_z, z);
        if (!idx.has_value()) {
            return false;
        }
        return add(*idx, std::move(payload));
    }

    // Read-only view of bin i.  Behaviour for i >= kBinCount is undefined
    // (callers iterate with i < kBinCount; AC-B06..B19 always do).
    std::span<const Payload> bin(std::size_t i) const noexcept {
        return std::span<const Payload>(bins_[i]);
    }

    // Number of payloads in bin i.
    std::size_t size(std::size_t i) const noexcept {
        return bins_[i].size();
    }

    // Sum of size(i) across all bins.  AC-B19 verifies this matches the
    // manual sum.
    std::size_t total_size() const noexcept {
        std::size_t total = 0;
        for (const auto& bin : bins_) {
            total += bin.size();
        }
        return total;
    }

    // Iterate bins in painter order: bin 0 first (far), bin kBinCount-1
    // last (near).  Within each bin, payloads are visited in insertion
    // order (D-InsertionOrder, no within-bin sort).  Each payload is
    // passed to visit as visit(payload).
    //
    // AC-B13 / AC-B14 / AC-Bfar / AC-Bnear pin this ordering.  Reversing
    // the loop direction inverts depth and breaks the painter algorithm.
    template <typename F>
    void for_each_back_to_front(F&& visit) const {
        for (std::size_t i = 0; i < kBinCount; ++i) {
            for (const auto& payload : bins_[i]) {
                visit(payload);
            }
        }
    }

private:
    std::array<std::vector<Payload>, kBinCount> bins_{};
};

}  // namespace render

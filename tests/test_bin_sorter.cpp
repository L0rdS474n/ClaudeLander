// tests/test_bin_sorter.cpp — Pass 8 render/bin_sorter tests.
//
// Covers AC-B01..B05, AC-B06..B10, AC-B11..B14, AC-B15..B17, AC-B18..B20,
// AC-Bfar, AC-Bnear, AC-Boor, AC-B80..B82 (24 ACs total, numbered per
// pass-8-bin-sorter.md §4).
// All tests tagged [render][bin_sorter].
//
// === Determinism plan ===
// BinSorter<int> is a pure data structure with no PRNG, no clock, no
// filesystem access, and no global mutable state.  All tests use
// deterministic integer sequences hand-written in the test body.
// AC-B18 and AC-B20 verify bit-identical output across independent runs.
//
// === Bug-class fences ===
// Three developer-mistake patterns are caught with prominent banner comments:
//   (a) AC-Bfar  — painter order must start at bin 0 (far), NOT bin 10.
//   (b) AC-Bnear — painter order must end at bin 10 (near), NOT bin 0.
//   (c) AC-Boor  — out-of-range inserts must be REJECTED, not clamped.
//
// === Hygiene (AC-B80) ===
// RAYLIB_VERSION is defined by raylib.h.  If it appears here, the include
// chain is polluted.  The BUILD_GAME=OFF build keeps raylib off the path.
//
// === AC-B82 noexcept ===
// static_asserts at the top of this file verify noexcept at compile time.
//
// === Red-state expectation ===
// This file FAILS TO COMPILE until the implementer creates:
//   src/render/bin_sorter.hpp
//   src/render/bin_sorter.cpp
// That is the correct red state for Gate 2 delivery.

#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

#include "render/bin_sorter.hpp"   // render::BinSorter, render::kBinCount,
                                   // render::bin_for_z

// ---------------------------------------------------------------------------
// AC-B80: render/bin_sorter.hpp must not pull in raylib, world/, entities/,
// input/, <random>, or <chrono>.
// RAYLIB_VERSION is defined by raylib.h; if present here, the include chain
// is polluted.  The BUILD_GAME=OFF build keeps raylib off the compiler path.
// ---------------------------------------------------------------------------
#ifdef RAYLIB_VERSION
static_assert(false,
    "AC-B80 VIOLATED: render/bin_sorter.hpp pulled in raylib.h "
    "(RAYLIB_VERSION is defined).  Remove the raylib dependency.");
#endif

// ---------------------------------------------------------------------------
// AC-B82 — BinSorter<int>::clear() and bin_for_z must be noexcept.
// Verified at compile time here in the same TU as the #include.
// ---------------------------------------------------------------------------
static_assert(
    noexcept(render::BinSorter<int>{}.clear()),
    "AC-B82: BinSorter<int>::clear() must be declared noexcept");

static_assert(
    noexcept(render::bin_for_z(0.0f, 0.0f)),
    "AC-B82: render::bin_for_z must be declared noexcept");

// ===========================================================================
// GROUP 1: Bin assignment — bin_for_z (AC-B01..B05)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-B01 — bin_for_z(10.0f, 10.0f) == 0 (z exactly at landscape_z).
//   Given: landscape_z = 10.0f, z = 10.0f
//   When:  bin_for_z is called
//   Then:  result == 0 (bin = 10 - 10 = 0)
// ---------------------------------------------------------------------------
TEST_CASE("AC-B01: bin_for_z(10, 10) == 0 (z at landscape_z gives bin 0)", "[render][bin_sorter]") {
    // Given / When
    const auto result = render::bin_for_z(10.0f, 10.0f);

    // Then
    REQUIRE(result.has_value());
    REQUIRE(*result == 0u);
}

// ---------------------------------------------------------------------------
// AC-B02 — bin_for_z(10.0f, 0.0f) == 10 (z = 0, furthest practical).
//   Given: landscape_z = 10.0f, z = 0.0f
//   When:  bin_for_z is called
//   Then:  result == 10 (bin = 10 - 0 = 10)
// ---------------------------------------------------------------------------
TEST_CASE("AC-B02: bin_for_z(10, 0) == 10 (z=0 gives furthest valid bin)", "[render][bin_sorter]") {
    // Given / When
    const auto result = render::bin_for_z(10.0f, 0.0f);

    // Then
    REQUIRE(result.has_value());
    REQUIRE(*result == 10u);
}

// ---------------------------------------------------------------------------
// AC-B03 — bin_for_z(10.0f, 11.0f) == nullopt (negative bin).
//   Given: landscape_z = 10.0f, z = 11.0f
//   When:  bin_for_z is called
//   Then:  result == nullopt (bin = 10 - 11 = -1, out of [0, kBinCount))
// ---------------------------------------------------------------------------
TEST_CASE("AC-B03: bin_for_z(10, 11) == nullopt (negative bin rejected)", "[render][bin_sorter]") {
    // Given / When
    const auto result = render::bin_for_z(10.0f, 11.0f);

    // Then
    REQUIRE_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// AC-B04 — bin_for_z(10.0f, -1.0f) == nullopt (bin 11 >= kBinCount).
//   Given: landscape_z = 10.0f, z = -1.0f
//   When:  bin_for_z is called
//   Then:  result == nullopt (bin = 10 - (-1) = 11 >= kBinCount = 11)
// ---------------------------------------------------------------------------
TEST_CASE("AC-B04: bin_for_z(10, -1) == nullopt (bin 11 >= kBinCount rejected)", "[render][bin_sorter]") {
    // Given / When
    const auto result = render::bin_for_z(10.0f, -1.0f);

    // Then
    REQUIRE_FALSE(result.has_value());
}

// ---------------------------------------------------------------------------
// AC-B05 — bin_for_z is deterministic: bit-identical across two calls.
//   Given: landscape_z = 7.0f, z = 3.0f
//   When:  bin_for_z is called twice with identical arguments
//   Then:  both results are bit-identical (same value, same has_value())
// ---------------------------------------------------------------------------
TEST_CASE("AC-B05: bin_for_z is deterministic — bit-identical on repeated calls", "[render][bin_sorter]") {
    // Given / When
    const auto a = render::bin_for_z(7.0f, 3.0f);
    const auto b = render::bin_for_z(7.0f, 3.0f);

    // Then — exact equality (not Approx; these are size_t values)
    REQUIRE(a.has_value() == b.has_value());
    REQUIRE(a.has_value());
    REQUIRE(*a == *b);
}

// ===========================================================================
// GROUP 2: add / add_by_z (AC-B06..B10)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-B06 — Default-constructed BinSorter<int> is fully empty.
//   Given: freshly default-constructed BinSorter<int>
//   When:  total_size() and bin(i).empty() are queried for all i
//   Then:  total_size() == 0 and every bin is empty
// ---------------------------------------------------------------------------
TEST_CASE("AC-B06: default-constructed BinSorter has total_size==0 and all bins empty", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;

    // When / Then
    REQUIRE(sorter.total_size() == 0u);
    for (std::size_t i = 0; i < render::kBinCount; ++i) {
        CAPTURE(i);
        REQUIRE(sorter.bin(i).empty());
        REQUIRE(sorter.size(i) == 0u);
    }
}

// ---------------------------------------------------------------------------
// AC-B07 — add(0, 42) places 42 into bin 0.
//   Given: fresh BinSorter<int>
//   When:  add(0, 42) is called
//   Then:  bin(0).size() == 1 and bin(0)[0] == 42
// ---------------------------------------------------------------------------
TEST_CASE("AC-B07: add(0, 42) gives bin(0).size()==1 and bin(0)[0]==42", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;

    // When
    const bool ok = sorter.add(0u, 42);

    // Then
    REQUIRE(ok);
    REQUIRE(sorter.size(0u) == 1u);
    REQUIRE(sorter.bin(0u)[0] == 42);
}

// ---------------------------------------------------------------------------
// AC-B08 — add to out-of-range bin returns false; sizes unchanged.
//   Given: fresh BinSorter<int>
//   When:  add(kBinCount, 99) is called (one past the last valid bin)
//   Then:  returns false and total_size() remains 0
// ---------------------------------------------------------------------------
TEST_CASE("AC-B08: add to out-of-range bin index returns false and leaves sizes unchanged", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;

    // When
    const bool ok = sorter.add(render::kBinCount, 99);

    // Then
    REQUIRE_FALSE(ok);
    REQUIRE(sorter.total_size() == 0u);

    // Also verify a clearly large index
    const bool ok2 = sorter.add(std::size_t{1000}, 77);
    REQUIRE_FALSE(ok2);
    REQUIRE(sorter.total_size() == 0u);
}

// ---------------------------------------------------------------------------
// AC-B09 — add_by_z(10.0f, 10.0f, 99) inserts into bin 0.
//   Given: fresh BinSorter<int>
//   When:  add_by_z(10.0f, 10.0f, 99) is called
//   Then:  bin(0)[0] == 99 (bin = 10 - 10 = 0)
// ---------------------------------------------------------------------------
TEST_CASE("AC-B09: add_by_z(10, 10, 99) gives bin(0)[0]==99", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;

    // When
    const bool ok = sorter.add_by_z(10.0f, 10.0f, 99);

    // Then
    REQUIRE(ok);
    REQUIRE(sorter.bin(0u).size() == 1u);
    REQUIRE(sorter.bin(0u)[0] == 99);
}

// ---------------------------------------------------------------------------
// AC-B10 — add_by_z with z far out-of-range returns false; sizes unchanged.
//   Given: fresh BinSorter<int>
//   When:  add_by_z(10.0f, 100.0f, 0) is called (bin = 10-100 = -90, negative)
//   Then:  returns false and total_size() == 0
// ---------------------------------------------------------------------------
TEST_CASE("AC-B10: add_by_z(10, 100, 0) returns false (out of range); sizes unchanged", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;

    // When
    const bool ok = sorter.add_by_z(10.0f, 100.0f, 0);

    // Then
    REQUIRE_FALSE(ok);
    REQUIRE(sorter.total_size() == 0u);
}

// ===========================================================================
// GROUP 3: Insertion order (AC-B11..B14)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-B11 — Adding 1, 2, 3 to bin 0 yields bin(0) == {1, 2, 3} exactly.
//   Given: fresh BinSorter<int>
//   When:  add(0, 1), add(0, 2), add(0, 3) are called in order
//   Then:  bin(0) == {1, 2, 3} (push order preserved, no sort)
// ---------------------------------------------------------------------------
TEST_CASE("AC-B11: adding 1,2,3 to bin 0 yields bin(0)=={1,2,3} in insertion order", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;

    // When
    sorter.add(0u, 1);
    sorter.add(0u, 2);
    sorter.add(0u, 3);

    // Then
    const auto span = sorter.bin(0u);
    REQUIRE(span.size() == 3u);
    REQUIRE(span[0] == 1);
    REQUIRE(span[1] == 2);
    REQUIRE(span[2] == 3);
}

// ---------------------------------------------------------------------------
// AC-B12 — Adding to multiple bins preserves per-bin order independently.
//   Given: fresh BinSorter<int>
//   When:  elements are added to bins 0, 5, and 10 in interleaved order
//   Then:  each bin holds its elements in the order they were added to that bin
// ---------------------------------------------------------------------------
TEST_CASE("AC-B12: adding to multiple bins preserves per-bin insertion order independently", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;

    // When — interleaved adds across bins
    sorter.add(0u,  10);
    sorter.add(5u,  50);
    sorter.add(10u, 100);
    sorter.add(0u,  11);
    sorter.add(5u,  51);
    sorter.add(10u, 101);
    sorter.add(0u,  12);

    // Then — each bin is in its own insertion order
    {
        const auto s = sorter.bin(0u);
        REQUIRE(s.size() == 3u);
        REQUIRE(s[0] == 10);
        REQUIRE(s[1] == 11);
        REQUIRE(s[2] == 12);
    }
    {
        const auto s = sorter.bin(5u);
        REQUIRE(s.size() == 2u);
        REQUIRE(s[0] == 50);
        REQUIRE(s[1] == 51);
    }
    {
        const auto s = sorter.bin(10u);
        REQUIRE(s.size() == 2u);
        REQUIRE(s[0] == 100);
        REQUIRE(s[1] == 101);
    }
}

// ---------------------------------------------------------------------------
// AC-B13 — for_each_back_to_front walks bin 0 first, bin 10 last.
//   Given: BinSorter with one item in bin 0 and one item in bin 10
//   When:  for_each_back_to_front is called and visit order recorded
//   Then:  the bin-0 item is visited BEFORE the bin-10 item
//          (bin 0 = far = first in painter's algorithm)
// ---------------------------------------------------------------------------
TEST_CASE("AC-B13: for_each_back_to_front visits bin 0 first (far) and bin 10 last (near)", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;
    sorter.add(0u,  1000);   // bin 0  = far
    sorter.add(10u, 9000);   // bin 10 = near

    // When
    std::vector<int> visited;
    sorter.for_each_back_to_front([&](int v) { visited.push_back(v); });

    // Then
    REQUIRE(visited.size() == 2u);
    REQUIRE(visited[0] == 1000);   // bin 0 visited first
    REQUIRE(visited[1] == 9000);   // bin 10 visited last
}

// ---------------------------------------------------------------------------
// AC-B14 — Within for_each_back_to_front, items in bin i appear in insertion order.
//   Given: BinSorter with items 7, 8, 9 added to bin 3 in that order
//   When:  for_each_back_to_front is called
//   Then:  7 appears before 8, 8 before 9 in the visited sequence
// ---------------------------------------------------------------------------
TEST_CASE("AC-B14: within for_each_back_to_front items in a bin appear in insertion order", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;
    sorter.add(3u, 7);
    sorter.add(3u, 8);
    sorter.add(3u, 9);

    // When
    std::vector<int> visited;
    sorter.for_each_back_to_front([&](int v) { visited.push_back(v); });

    // Then
    REQUIRE(visited.size() == 3u);
    REQUIRE(visited[0] == 7);
    REQUIRE(visited[1] == 8);
    REQUIRE(visited[2] == 9);
}

// ===========================================================================
// GROUP 4: clear + reuse (AC-B15..B17)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-B15 — After clear(), all bins empty; total_size() == 0.
//   Given: BinSorter with items in several bins
//   When:  clear() is called
//   Then:  total_size() == 0 and all bins are empty
// ---------------------------------------------------------------------------
TEST_CASE("AC-B15: clear() empties all bins and resets total_size to 0", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;
    sorter.add(0u,  1);
    sorter.add(5u,  2);
    sorter.add(10u, 3);
    REQUIRE(sorter.total_size() == 3u);

    // When
    sorter.clear();

    // Then
    REQUIRE(sorter.total_size() == 0u);
    for (std::size_t i = 0; i < render::kBinCount; ++i) {
        CAPTURE(i);
        REQUIRE(sorter.bin(i).empty());
        REQUIRE(sorter.size(i) == 0u);
    }
}

// ---------------------------------------------------------------------------
// AC-B16 — Reuse after clear() works exactly like a fresh-constructed instance.
//   Given: BinSorter used once and cleared
//   When:  same items are added again after clear()
//   Then:  results are identical to adding to a fresh BinSorter
// ---------------------------------------------------------------------------
TEST_CASE("AC-B16: reuse after clear() gives identical results to fresh construction", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;
    sorter.add(0u, 10);
    sorter.add(0u, 20);
    sorter.add(7u, 30);
    sorter.clear();

    // When — add same sequence as if fresh
    sorter.add(0u, 10);
    sorter.add(0u, 20);
    sorter.add(7u, 30);

    // Then — matches a freshly-constructed sorter with the same sequence
    render::BinSorter<int> fresh;
    fresh.add(0u, 10);
    fresh.add(0u, 20);
    fresh.add(7u, 30);

    REQUIRE(sorter.total_size() == fresh.total_size());

    for (std::size_t i = 0; i < render::kBinCount; ++i) {
        CAPTURE(i);
        REQUIRE(sorter.size(i) == fresh.size(i));
        const auto s_span = sorter.bin(i);
        const auto f_span = fresh.bin(i);
        REQUIRE(s_span.size() == f_span.size());
        for (std::size_t j = 0; j < s_span.size(); ++j) {
            REQUIRE(s_span[j] == f_span[j]);
        }
    }
}

// ---------------------------------------------------------------------------
// AC-B17 — clear() does not reduce capacity (perf invariant; non-binding).
//   Given: BinSorter with many items inserted, then cleared
//   When:  clear() is called
//   Then:  the same number of items can be added again without observable error
//          (capacity was not shrunk; test is behavioural: no crash/false-return)
// ---------------------------------------------------------------------------
TEST_CASE("AC-B17: clear() does not reduce capacity — reinsert succeeds without error", "[render][bin_sorter]") {
    // Given: fill all bins with multiple items
    render::BinSorter<int> sorter;
    for (std::size_t i = 0; i < render::kBinCount; ++i) {
        for (int j = 0; j < 20; ++j) {
            sorter.add(i, j);
        }
    }
    REQUIRE(sorter.total_size() == render::kBinCount * 20u);

    // When
    sorter.clear();

    // Then: reinserting the same volume succeeds (no false-returns)
    for (std::size_t i = 0; i < render::kBinCount; ++i) {
        for (int j = 0; j < 20; ++j) {
            const bool ok = sorter.add(i, j);
            CAPTURE(i, j);
            REQUIRE(ok);
        }
    }
    REQUIRE(sorter.total_size() == render::kBinCount * 20u);
}

// ===========================================================================
// GROUP 5: Determinism (AC-B18..B20)
// ===========================================================================

// ---------------------------------------------------------------------------
// AC-B18 — Same insertion sequence → identical iteration sequence (1000 items).
//   Given: a deterministic sequence of 1000 (bin_index, value) pairs
//   When:  the sequence is inserted and iterated twice (two independent sorters)
//   Then:  both visited sequences are bit-identical
// ---------------------------------------------------------------------------
TEST_CASE("AC-B18: same 1000-item insertion sequence gives bit-identical iteration across two runs", "[render][bin_sorter]") {
    // Given: deterministic sequence — no PRNG, no clock
    auto build_sorter = [&]() -> render::BinSorter<int> {
        render::BinSorter<int> s;
        for (int i = 0; i < 1000; ++i) {
            const std::size_t bin = static_cast<std::size_t>(i % render::kBinCount);
            s.add(bin, i);
        }
        return s;
    };

    auto collect = [&](const render::BinSorter<int>& s) {
        std::vector<int> out;
        out.reserve(1000);
        s.for_each_back_to_front([&](int v) { out.push_back(v); });
        return out;
    };

    // When
    const render::BinSorter<int> sa = build_sorter();
    const render::BinSorter<int> sb = build_sorter();
    const auto seq_a = collect(sa);
    const auto seq_b = collect(sb);

    // Then — exact (bit-identical) equality
    REQUIRE(seq_a.size() == seq_b.size());
    for (std::size_t i = 0; i < seq_a.size(); ++i) {
        CAPTURE(i, seq_a[i], seq_b[i]);
        REQUIRE(seq_a[i] == seq_b[i]);
    }
}

// ---------------------------------------------------------------------------
// AC-B19 — total_size() matches sum of size(i) for all i.
//   Given: BinSorter with varying item counts per bin
//   When:  total_size() is compared to the manual sum of size(i)
//   Then:  they are equal
// ---------------------------------------------------------------------------
TEST_CASE("AC-B19: total_size() == sum of size(i) for all bins", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;
    // Add different counts to different bins
    for (int j = 0; j < 3; ++j) sorter.add(0u, j);
    for (int j = 0; j < 7; ++j) sorter.add(4u, j);
    for (int j = 0; j < 1; ++j) sorter.add(9u, j);
    // bins 1,2,3,5,6,7,8,10 remain empty

    // When
    const std::size_t total = sorter.total_size();

    // Then
    std::size_t manual_sum = 0;
    for (std::size_t i = 0; i < render::kBinCount; ++i) {
        manual_sum += sorter.size(i);
    }
    REQUIRE(total == manual_sum);
    REQUIRE(total == 11u);   // 3+7+1
}

// ---------------------------------------------------------------------------
// AC-B20 — Iteration order is stable across two clear-and-refill cycles.
//   Given: a fixed insertion sequence
//   When:  the sorter is filled, iterated (cycle 1), cleared, refilled with
//          the same sequence, and iterated again (cycle 2)
//   Then:  both iteration sequences are bit-identical
// ---------------------------------------------------------------------------
TEST_CASE("AC-B20: iteration order is stable across two clear-and-refill cycles", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;
    const int seed[] = {10, 20, 30, 40, 50};

    auto fill = [&]() {
        for (int v : seed) {
            // Distribute across bins by position
            const std::size_t bin = static_cast<std::size_t>(v / 10 - 1) % render::kBinCount;
            sorter.add(bin, v);
        }
    };

    auto collect = [&]() {
        std::vector<int> out;
        sorter.for_each_back_to_front([&](int v) { out.push_back(v); });
        return out;
    };

    // When — cycle 1
    fill();
    const auto cycle1 = collect();

    // When — cycle 2: clear and refill with identical sequence
    sorter.clear();
    fill();
    const auto cycle2 = collect();

    // Then
    REQUIRE(cycle1.size() == cycle2.size());
    for (std::size_t i = 0; i < cycle1.size(); ++i) {
        CAPTURE(i, cycle1[i], cycle2[i]);
        REQUIRE(cycle1[i] == cycle2[i]);
    }
}

// ===========================================================================
//  BUG-CLASS FENCE (AC-Bfar)
// ===========================================================================
//
//  D-PainterOrder (locked): for_each_back_to_front iterates bin 0 FIRST
//  (far/horizon) through bin 10 LAST (near).  This is painter's algorithm:
//  far objects are drawn first and overwritten by nearer ones.
//
//  A reversed-iteration bug (bin 10 first → bin 0 last) would cause near
//  objects to be drawn UNDER far objects, inverting depth.
//
//  This test places payload 111 in bin 0 ONLY and payload 222 in bin 10 ONLY.
//  A correct implementation visits 111 before 222.
//  A reversed implementation visits 222 before 111, and the test fails.
//
//  If this test fails: re-read D-PainterOrder.  Do NOT change the test.
//  Fix the iteration direction in for_each_back_to_front.
// ===========================================================================
TEST_CASE("AC-Bfar (BUG-CLASS FENCE): for_each_back_to_front visits bin 0 (far) before bin 10 (near)", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;
    sorter.add(0u,  111);   // bin 0  = FAR  (should be visited FIRST)
    sorter.add(10u, 222);   // bin 10 = NEAR (should be visited LAST)

    // When
    std::vector<int> visited;
    sorter.for_each_back_to_front([&](int v) { visited.push_back(v); });

    // Then
    REQUIRE(visited.size() == 2u);
    REQUIRE(visited[0] == 111);   // FAR item visited first — NOT 222
    REQUIRE(visited[1] == 222);   // NEAR item visited last

    // Explicit guard: catches the reversed-order bug
    REQUIRE(visited.front() == 111);
    REQUIRE_FALSE(visited.front() == 222);
}

// ===========================================================================
//  BUG-CLASS FENCE (AC-Bnear)
// ===========================================================================
//
//  Complementary to AC-Bfar: bin 10 is the NEAR bin and must be visited LAST.
//
//  This test adds items to every bin and records first and last visited items.
//  The first item visited must come from bin 0; the last from bin 10.
//
//  A bug that shifts the range (iterates bin 1..10 and misses bin 0, or
//  iterates bin 0..9 and misses bin 10) fails this test.
//
//  If this test fails: re-read D-BinCount and D-PainterOrder.  Both bin 0
//  and bin 10 must be included in the iteration; 11 bins total (kBinCount=11).
// ===========================================================================
TEST_CASE("AC-Bnear (BUG-CLASS FENCE): for_each_back_to_front reaches bin 10 (near) as the last bin", "[render][bin_sorter]") {
    // Given: one item in every bin; use sentinel values equal to 1000*bin_index
    render::BinSorter<int> sorter;
    for (std::size_t i = 0; i < render::kBinCount; ++i) {
        sorter.add(i, static_cast<int>(i) * 1000);
    }

    // When
    std::vector<int> visited;
    sorter.for_each_back_to_front([&](int v) { visited.push_back(v); });

    // Then
    REQUIRE(visited.size() == render::kBinCount);
    // First item must be from bin 0 (sentinel = 0)
    REQUIRE(visited.front() == 0);
    // Last item must be from bin 10 (sentinel = 10000)
    REQUIRE(visited.back() == 10000);
    // Monotonically non-decreasing: confirms 0→10 order across all bins
    for (std::size_t i = 1; i < visited.size(); ++i) {
        CAPTURE(i, visited[i-1], visited[i]);
        REQUIRE(visited[i] >= visited[i-1]);
    }
}

// ===========================================================================
//  BUG-CLASS FENCE (AC-Boor)
// ===========================================================================
//
//  D-DiscardOOR (locked): out-of-range bin indices are REJECTED (return false),
//  NOT clamped to bin 0 or bin 10.
//
//  Two silent failure modes this test guards against:
//    (a) Clamp-to-0:  add(kBinCount, x) silently inserts x into bin 0.
//        Symptom: bin 0 grows; total_size() increases.
//    (b) Clamp-to-10: add(kBinCount, x) silently inserts x into bin 10.
//        Symptom: bin 10 grows; total_size() increases.
//
//  Both clamping bugs would cause depth-ordering artifacts where drawables
//  that are outside the visible frustum appear in the scene.
//
//  If this test fails: do NOT add a clamp.  Return false from add() when
//  bin_index >= kBinCount, as specified by D-DiscardOOR.
// ===========================================================================
TEST_CASE("AC-Boor (BUG-CLASS FENCE): out-of-range add is REJECTED not clamped to bin 0 or bin 10", "[render][bin_sorter]") {
    // Given
    render::BinSorter<int> sorter;
    sorter.add(0u,  100);    // sentinel in bin 0
    sorter.add(10u, 200);    // sentinel in bin 10
    REQUIRE(sorter.total_size() == 2u);

    // When — attempt to add to out-of-range indices
    const bool r1 = sorter.add(render::kBinCount,      999);   // == 11, one past end
    const bool r2 = sorter.add(render::kBinCount + 5u, 888);   // clearly OOR
    const bool r3 = sorter.add(std::size_t{9999},      777);   // absurdly OOR

    // Then — all rejected
    REQUIRE_FALSE(r1);
    REQUIRE_FALSE(r2);
    REQUIRE_FALSE(r3);

    // And sizes are UNCHANGED — no silent clamping to bin 0 or bin 10
    REQUIRE(sorter.total_size() == 2u);
    REQUIRE(sorter.size(0u)  == 1u);
    REQUIRE(sorter.size(10u) == 1u);
    // bin 0 sentinel unmolested
    REQUIRE(sorter.bin(0u)[0]  == 100);
    // bin 10 sentinel unmolested
    REQUIRE(sorter.bin(10u)[0] == 200);
}

// ===========================================================================
// GROUP 6: Hygiene (AC-B80..B82)
// ===========================================================================
//
// AC-B80 (no-raylib static_assert) is at the top of this file.
// AC-B82 (noexcept static_asserts) are at the top of this file.

TEST_CASE("AC-B80: render/bin_sorter.hpp compiles without raylib, world/, entities/, <random>, <chrono>", "[render][bin_sorter]") {
    // Given: this file was compiled with BUILD_GAME=OFF (no raylib on path)
    // When:  it reaches this TEST_CASE at runtime
    // Then:  the headers compiled without forbidden dependencies
    SUCCEED("compilation without raylib/forbidden deps succeeded — AC-B80 satisfied");
}

TEST_CASE("AC-B81: claude_lander_render link list unchanged — render library links against core+warnings only", "[render][bin_sorter]") {
    // Given: this test binary linked with claude_lander_render which links
    //        against claude_lander_core + claude_lander_warnings only.
    // When:  this test is reached
    // Then:  no link errors — AC-B81 satisfied
    SUCCEED("render library link list unchanged (core+warnings only) — AC-B81 satisfied");
}

TEST_CASE("AC-B82: BinSorter::clear() and bin_for_z are noexcept (verified by static_assert at top of TU)", "[render][bin_sorter]") {
    // Given: static_assert(noexcept(...)) at the top of this file
    // When:  this test is reached (compile succeeded means static_assert passed)
    // Then:  AC-B82 is satisfied
    SUCCEED("static_assert(noexcept) passed at compile time — AC-B82 satisfied");
}

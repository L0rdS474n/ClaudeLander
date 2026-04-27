// Pass 0 smoke test -- proves Catch2 v3 is wired up via FetchContent and
// that ctest can discover and run cases. Real characterisation tests for
// each module land in Pass 1+ alongside the modules themselves.

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Catch2 toolchain is alive", "[smoke]") {
    REQUIRE(1 + 1 == 2);
}

TEST_CASE("C++20 features compile", "[smoke]") {
    constexpr int kAnswer = 42;
    REQUIRE(kAnswer == 42);
}

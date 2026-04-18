#include <catch2/catch_all.hpp>
#include <fsrs/detail/fuzz.hpp>

#include <random>

using fsrs::detail::FuzzRange;
using fsrs::detail::FUZZ_RANGES;
using fsrs::detail::get_fuzzed_interval;

TEST_CASE("FUZZ_RANGES has 3 entries matching py-fsrs", "[fuzz]") {
    STATIC_REQUIRE(FUZZ_RANGES.size() == 3);
    REQUIRE(FUZZ_RANGES[0].start == 2.5);
    REQUIRE(FUZZ_RANGES[0].end   == 7.0);
    REQUIRE(FUZZ_RANGES[0].factor == 0.15);
    REQUIRE(FUZZ_RANGES[1].start == 7.0);
    REQUIRE(FUZZ_RANGES[1].end   == 20.0);
    REQUIRE(FUZZ_RANGES[1].factor == 0.1);
    REQUIRE(FUZZ_RANGES[2].start == 20.0);
    REQUIRE(FUZZ_RANGES[2].factor == 0.05);
}

TEST_CASE("Intervals < 2.5 days are not fuzzed", "[fuzz]") {
    std::mt19937 rng{42};
    REQUIRE(get_fuzzed_interval(1, rng, 36500) == 1);
    REQUIRE(get_fuzzed_interval(2, rng, 36500) == 2);
}

TEST_CASE("Fuzzed interval stays within documented window", "[fuzz]") {
    std::mt19937 rng{42};
    for (int trial = 0; trial < 200; ++trial) {
        int out = get_fuzzed_interval(50, rng, 36500);
        // With interval=50, delta ≈ 1 + 0.15*(7-2.5) + 0.1*(20-7) + 0.05*(50-20)
        //                       = 1 + 0.675 + 1.3 + 1.5 = 4.475
        // min_ivl = round(50-4.475) = 46, max_ivl = round(50+4.475) = 54 (or 55)
        REQUIRE(out >= 45);
        REQUIRE(out <= 55);
    }
}

TEST_CASE("Fuzzed interval respects maximum_interval cap", "[fuzz]") {
    std::mt19937 rng{42};
    int out = get_fuzzed_interval(100, rng, /*max_interval=*/50);
    REQUIRE(out <= 50);
}

TEST_CASE("Same seed produces same fuzz sequence", "[fuzz]") {
    std::mt19937 a{42}, b{42};
    for (int i = 0; i < 10; ++i) {
        REQUIRE(get_fuzzed_interval(50, a, 36500) ==
                get_fuzzed_interval(50, b, 36500));
    }
}

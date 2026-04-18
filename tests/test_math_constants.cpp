#include <catch2/catch_all.hpp>
#include <fsrs/detail/math.hpp>

#include <array>

using namespace fsrs::detail;

TEST_CASE("DEFAULT_PARAMETERS has 21 entries", "[math]") {
    STATIC_REQUIRE(DEFAULT_PARAMETERS.size() == 21);
}

TEST_CASE("DEFAULT_PARAMETERS match py-fsrs values", "[math]") {
    // Verbatim from third_party/py-fsrs/fsrs/scheduler.py:30-52
    constexpr std::array<double, 21> expected{
        0.212, 1.2931, 2.3065, 8.2956, 6.4133, 0.8334, 3.0194,
        0.001, 1.8722, 0.1666, 0.796,  1.4835, 0.0614, 0.2629,
        1.6483, 0.6014, 1.8729, 0.5425, 0.0912, 0.0658, 0.1542,
    };
    for (size_t i = 0; i < 21; ++i) {
        REQUIRE(DEFAULT_PARAMETERS[i] == expected[i]);
    }
}

TEST_CASE("LOWER_BOUNDS_PARAMETERS match py-fsrs", "[math]") {
    // Verbatim from third_party/py-fsrs/fsrs/scheduler.py:55-77
    constexpr std::array<double, 21> expected{
        STABILITY_MIN, STABILITY_MIN, STABILITY_MIN, STABILITY_MIN,
        1.0, 0.001, 0.001, 0.001, 0.0, 0.0, 0.001,
        0.001, 0.001, 0.001, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.1,
    };
    for (size_t i = 0; i < 21; ++i) {
        REQUIRE(LOWER_BOUNDS_PARAMETERS[i] == expected[i]);
    }
}

TEST_CASE("UPPER_BOUNDS_PARAMETERS match py-fsrs", "[math]") {
    // Verbatim from third_party/py-fsrs/fsrs/scheduler.py:80-102
    constexpr std::array<double, 21> expected{
        INITIAL_STABILITY_MAX, INITIAL_STABILITY_MAX,
        INITIAL_STABILITY_MAX, INITIAL_STABILITY_MAX,
        10.0, 4.0, 4.0, 0.75, 4.5, 0.8, 3.5,
        5.0, 0.25, 0.9, 4.0, 1.0, 6.0, 2.0, 2.0, 0.8, 0.8,
    };
    for (size_t i = 0; i < 21; ++i) {
        REQUIRE(UPPER_BOUNDS_PARAMETERS[i] == expected[i]);
    }
}

TEST_CASE("Scalar constants", "[math]") {
    STATIC_REQUIRE(STABILITY_MIN == 0.001);
    STATIC_REQUIRE(INITIAL_STABILITY_MAX == 100.0);
    STATIC_REQUIRE(MIN_DIFFICULTY == 1.0);
    STATIC_REQUIRE(MAX_DIFFICULTY == 10.0);
    STATIC_REQUIRE(FSRS_DEFAULT_DECAY == 0.1542);
}

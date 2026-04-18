#include <catch2/catch_all.hpp>
#include <fsrs/scheduler.hpp>

#include <cmath>

using Catch::Approx;

TEST_CASE("initial_stability matches py-fsrs", "[helpers]") {
    fsrs::Scheduler s;
    // py-fsrs _initial_stability(Rating.Again) = parameters[0] = 0.212
    REQUIRE(s.initial_stability(fsrs::Rating::Again) == Approx(0.212));
    REQUIRE(s.initial_stability(fsrs::Rating::Hard)  == Approx(1.2931));
    REQUIRE(s.initial_stability(fsrs::Rating::Good)  == Approx(2.3065));
    REQUIRE(s.initial_stability(fsrs::Rating::Easy)  == Approx(8.2956));
}

TEST_CASE("initial_difficulty clamped matches py-fsrs", "[helpers]") {
    fsrs::Scheduler s;
    // py-fsrs _initial_difficulty(Rating.Good, clamp=True)
    //   = parameters[4] - e^(parameters[5] * (rating-1)) + 1
    const auto& p = s.parameters();
    const double expected = p[4] - std::exp(p[5] * (3.0 - 1.0)) + 1.0;
    REQUIRE(s.initial_difficulty(fsrs::Rating::Good, /*clamp=*/true)
            == Approx(expected).margin(1e-9));
}

TEST_CASE("next_interval clamps to [1, maximum_interval]", "[helpers]") {
    fsrs::Scheduler s;
    REQUIRE(s.next_interval(0.0) == 1);
    REQUIRE(s.next_interval(1.0) >= 1);
    REQUIRE(s.next_interval(1.0e9) == s.maximum_interval());
}

TEST_CASE("clamp_difficulty clamps to [1, 10]", "[helpers]") {
    fsrs::Scheduler s;
    REQUIRE(s.clamp_difficulty(0.5)  == Approx(1.0));
    REQUIRE(s.clamp_difficulty(5.0)  == Approx(5.0));
    REQUIRE(s.clamp_difficulty(11.0) == Approx(10.0));
}

TEST_CASE("clamp_stability clamps to >= STABILITY_MIN", "[helpers]") {
    fsrs::Scheduler s;
    REQUIRE(s.clamp_stability(0.0)  == Approx(fsrs::detail::STABILITY_MIN));
    REQUIRE(s.clamp_stability(10.0) == Approx(10.0));
}

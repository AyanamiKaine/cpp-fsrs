#include <catch2/catch_all.hpp>
#include <fsrs/scheduler.hpp>
#include <fsrs/card.hpp>

#include <chrono>

using Catch::Approx;
using namespace std::chrono_literals;

TEST_CASE("Retrievability is 0 for fresh card (no last_review)", "[retrievability]") {
    fsrs::Scheduler s;
    fsrs::Card c;  // last_review = nullopt
    REQUIRE(s.get_card_retrievability(c) == 0.0);
}

TEST_CASE("Retrievability is 0 when stability is nullopt", "[retrievability]") {
    fsrs::Scheduler s;
    fsrs::Card c;
    c.last_review = std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now());
    // stability still nullopt
    REQUIRE(s.get_card_retrievability(c) == 0.0);
}

TEST_CASE("Retrievability is 1.0 when elapsed_days == 0", "[retrievability]") {
    fsrs::Scheduler s;
    fsrs::Card c;
    c.stability   = 10.0;
    const auto now = std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now());
    c.last_review = now;
    REQUIRE(s.get_card_retrievability(c, now) == Approx(1.0).margin(1e-9));
}

TEST_CASE("Retrievability decreases with elapsed time", "[retrievability]") {
    fsrs::Scheduler s;
    fsrs::Card c;
    c.stability = 10.0;
    using namespace std::chrono;
    const sys_time<microseconds> t0 = time_point_cast<microseconds>(system_clock::now());
    c.last_review = t0;
    const double r10 = s.get_card_retrievability(c, t0 + days{10});
    const double r20 = s.get_card_retrievability(c, t0 + days{20});
    REQUIRE(r10 > r20);
    REQUIRE(r10 < 1.0);
    REQUIRE(r20 > 0.0);
}

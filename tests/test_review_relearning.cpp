#include <catch2/catch_all.hpp>
#include <fsrs/scheduler.hpp>

#include <chrono>

using Catch::Approx;
using namespace std::chrono_literals;

// Reach Relearning by Easy → Review, then Again.
static fsrs::Card lapsed_card(fsrs::Scheduler& s, fsrs::Card::TimePoint t) {
    fsrs::Card c;
    auto [a, _]  = s.review_card(c, fsrs::Rating::Easy,  t);                        // → Review
    auto [b, __] = s.review_card(a, fsrs::Rating::Again, t + std::chrono::days{5}); // → Relearning
    return b;
}

TEST_CASE("Relearning Good on last step graduates to Review", "[review][relearning]") {
    fsrs::Scheduler::Config cfg; cfg.enable_fuzzing = false;
    fsrs::Scheduler s{cfg};

    const auto t0 = std::chrono::sys_days{std::chrono::year{2025}/1/1};
    auto c = lapsed_card(s, t0);
    REQUIRE(c.state == fsrs::State::Relearning);
    REQUIRE(*c.step == 0);
    REQUIRE(s.relearning_steps().size() == 1);

    auto [c1, _] = s.review_card(c, fsrs::Rating::Good, t0 + std::chrono::days{5} + 10min);
    REQUIRE(c1.state == fsrs::State::Review);
    REQUIRE_FALSE(c1.step.has_value());
}

TEST_CASE("Relearning Again resets step to 0", "[review][relearning]") {
    fsrs::Scheduler::Config cfg;
    cfg.enable_fuzzing   = false;
    cfg.relearning_steps = {10min, 20min};  // two steps, to have something to reset
    fsrs::Scheduler s{cfg};

    const auto t0 = std::chrono::sys_days{std::chrono::year{2025}/1/1};
    auto c = lapsed_card(s, t0);
    auto [c1, _]  = s.review_card(c,  fsrs::Rating::Good,  t0 + std::chrono::days{5} + 10min);
    REQUIRE(c1.state == fsrs::State::Relearning);
    REQUIRE(*c1.step == 1);

    auto [c2, __] = s.review_card(c1, fsrs::Rating::Again, t0 + std::chrono::days{5} + 20min);
    REQUIRE(c2.state == fsrs::State::Relearning);
    REQUIRE(*c2.step == 0);
}

TEST_CASE("Relearning Easy graduates immediately", "[review][relearning]") {
    fsrs::Scheduler::Config cfg; cfg.enable_fuzzing = false;
    fsrs::Scheduler s{cfg};

    const auto t0 = std::chrono::sys_days{std::chrono::year{2025}/1/1};
    auto c = lapsed_card(s, t0);
    auto [c1, _] = s.review_card(c, fsrs::Rating::Easy, t0 + std::chrono::days{5} + 10min);
    REQUIRE(c1.state == fsrs::State::Review);
}

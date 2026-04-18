#include <catch2/catch_all.hpp>
#include <fsrs/scheduler.hpp>

#include <chrono>

using Catch::Approx;
using namespace std::chrono_literals;

// Helper: graduate card to Review state via Easy rating.
static fsrs::Card graduated_card(fsrs::Scheduler& s, fsrs::Card::TimePoint t) {
    fsrs::Card c;
    auto [out, _] = s.review_card(c, fsrs::Rating::Easy, t);
    return out;
}

TEST_CASE("Review-state Good updates stability using next_stability", "[review][review-state]") {
    fsrs::Scheduler::Config cfg; cfg.enable_fuzzing = false;
    fsrs::Scheduler s{cfg};

    const auto t0 = std::chrono::sys_days{std::chrono::year{2025}/1/1};
    auto c = graduated_card(s, t0);
    REQUIRE(c.state == fsrs::State::Review);

    const auto t1 = t0 + std::chrono::days{5};
    auto [c1, _] = s.review_card(c, fsrs::Rating::Good, t1);

    REQUIRE(c1.state == fsrs::State::Review);
    REQUIRE(c1.stability.has_value());
    REQUIRE(*c1.stability > *c.stability);  // Good review should increase stability
}

TEST_CASE("Review-state Again without relearning_steps stays in Review", "[review][review-state]") {
    fsrs::Scheduler::Config cfg;
    cfg.enable_fuzzing    = false;
    cfg.relearning_steps  = {};
    fsrs::Scheduler s{cfg};

    const auto t0 = std::chrono::sys_days{std::chrono::year{2025}/1/1};
    auto c = graduated_card(s, t0);
    auto [c1, _] = s.review_card(c, fsrs::Rating::Again, t0 + std::chrono::days{10});
    REQUIRE(c1.state == fsrs::State::Review);
}

TEST_CASE("Review-state Again with relearning_steps moves to Relearning", "[review][review-state]") {
    fsrs::Scheduler::Config cfg;
    cfg.enable_fuzzing = false;  // default relearning_steps = {10min}
    fsrs::Scheduler s{cfg};

    const auto t0 = std::chrono::sys_days{std::chrono::year{2025}/1/1};
    auto c = graduated_card(s, t0);
    auto [c1, _] = s.review_card(c, fsrs::Rating::Again, t0 + std::chrono::days{10});
    REQUIRE(c1.state == fsrs::State::Relearning);
    REQUIRE(c1.step.has_value());
    REQUIRE(*c1.step == 0);
}

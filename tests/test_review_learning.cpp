#include <catch2/catch_all.hpp>
#include <fsrs/scheduler.hpp>

#include <chrono>

using Catch::Approx;
using namespace std::chrono_literals;

TEST_CASE("First review in Learning state sets initial stability/difficulty", "[review][learning]") {
    fsrs::Scheduler::Config cfg;
    cfg.enable_fuzzing = false;
    fsrs::Scheduler s{cfg};

    fsrs::Card c;  // Learning, step=0, no stability/difficulty
    const auto when = std::chrono::sys_days{std::chrono::year{2025}/1/1};

    auto [card, log] = s.review_card(c, fsrs::Rating::Good, when);

    REQUIRE(card.stability.has_value());
    REQUIRE(*card.stability  == Approx(2.3065));  // initial_stability for Good
    REQUIRE(card.difficulty.has_value());
    REQUIRE(card.last_review == when);
    REQUIRE(log.card_id      == c.card_id);
    REQUIRE(log.rating       == fsrs::Rating::Good);
}

TEST_CASE("Rating.Good with one remaining step advances to next step", "[review][learning]") {
    fsrs::Scheduler::Config cfg;
    cfg.enable_fuzzing = false;
    fsrs::Scheduler s{cfg};

    fsrs::Card c;  // step = 0, learning_steps = [1min, 10min]
    const auto when = std::chrono::sys_days{std::chrono::year{2025}/1/1};

    auto [card, log] = s.review_card(c, fsrs::Rating::Good, when);
    REQUIRE(card.state == fsrs::State::Learning);
    REQUIRE(card.step.has_value());
    REQUIRE(*card.step == 1);
}

TEST_CASE("Rating.Good on final learning step graduates to Review", "[review][learning]") {
    fsrs::Scheduler::Config cfg;
    cfg.enable_fuzzing = false;
    fsrs::Scheduler s{cfg};

    fsrs::Card c;
    const auto t0 = std::chrono::sys_days{std::chrono::year{2025}/1/1};

    // Two Good reviews: step 0→1 (still Learning), step 1→graduate
    auto [c1, _] = s.review_card(c,  fsrs::Rating::Good, t0);
    auto [c2, __] = s.review_card(c1, fsrs::Rating::Good, t0 + std::chrono::minutes{1});

    REQUIRE(c2.state == fsrs::State::Review);
    REQUIRE_FALSE(c2.step.has_value());
}

TEST_CASE("Rating.Easy in Learning graduates immediately", "[review][learning]") {
    fsrs::Scheduler::Config cfg;
    cfg.enable_fuzzing = false;
    fsrs::Scheduler s{cfg};

    fsrs::Card c;
    auto [card, log] = s.review_card(c, fsrs::Rating::Easy,
        std::chrono::sys_days{std::chrono::year{2025}/1/1});
    REQUIRE(card.state == fsrs::State::Review);
    REQUIRE_FALSE(card.step.has_value());
}

TEST_CASE("Rating.Again in Learning resets step to 0", "[review][learning]") {
    fsrs::Scheduler::Config cfg;
    cfg.enable_fuzzing = false;
    fsrs::Scheduler s{cfg};

    fsrs::Card c;
    const auto t0 = std::chrono::sys_days{std::chrono::year{2025}/1/1};
    auto [c1, _] = s.review_card(c, fsrs::Rating::Good,  t0);           // step → 1
    auto [c2, __] = s.review_card(c1, fsrs::Rating::Again, t0 + 1min);  // step → 0

    REQUIRE(c2.state == fsrs::State::Learning);
    REQUIRE(c2.step.has_value());
    REQUIRE(*c2.step == 0);
}

TEST_CASE("review_card preserves card_id", "[review][learning]") {
    fsrs::Scheduler s;
    fsrs::Card c;
    c.card_id = 12345;
    auto [card, log] = s.review_card(c, fsrs::Rating::Good,
        std::chrono::sys_days{std::chrono::year{2025}/1/1});
    REQUIRE(card.card_id == 12345);
    REQUIRE(log.card_id  == 12345);
}

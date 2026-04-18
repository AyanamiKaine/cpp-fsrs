#include <catch2/catch_all.hpp>
#include <fsrs/scheduler.hpp>

#include <chrono>
#include <stdexcept>

using namespace std::chrono_literals;

TEST_CASE("reschedule_card throws on mismatched card_id", "[reschedule]") {
    fsrs::Scheduler s;
    fsrs::Card c;
    c.card_id = 1;
    fsrs::ReviewLog bad{
        .card_id = 999, .rating = fsrs::Rating::Good,
        .review_datetime = {}, .review_duration = std::nullopt,
    };
    REQUIRE_THROWS_AS(s.reschedule_card(c, {bad}), std::invalid_argument);
}

TEST_CASE("reschedule_card with no logs is a no-op", "[reschedule]") {
    fsrs::Scheduler s;
    fsrs::Card c;
    const auto original_due = c.due;
    auto result = s.reschedule_card(c, {});
    REQUIRE(result.card_id == c.card_id);
    REQUIRE(result.due     == original_due);
}

TEST_CASE("reschedule_card replays logs regardless of order", "[reschedule]") {
    fsrs::Scheduler::Config cfg; cfg.enable_fuzzing = false;
    fsrs::Scheduler s{cfg};

    fsrs::Card c;
    const auto t0 = std::chrono::sys_time<std::chrono::microseconds>{
        std::chrono::sys_days{std::chrono::year{2025}/1/1}};
    fsrs::ReviewLog l1{c.card_id, fsrs::Rating::Good, t0,                 std::nullopt};
    fsrs::ReviewLog l2{c.card_id, fsrs::Rating::Good, t0 + 1min,          std::nullopt};
    fsrs::ReviewLog l3{c.card_id, fsrs::Rating::Good, t0 + 10min + 1min,  std::nullopt};

    auto forward  = s.reschedule_card(c, {l1, l2, l3});
    auto shuffled = s.reschedule_card(c, {l3, l1, l2});

    REQUIRE(forward.state      == shuffled.state);
    REQUIRE(forward.stability  == shuffled.stability);
    REQUIRE(forward.difficulty == shuffled.difficulty);
    REQUIRE(forward.due        == shuffled.due);
}

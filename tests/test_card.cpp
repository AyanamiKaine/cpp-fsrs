#include <catch2/catch_all.hpp>
#include <fsrs/card.hpp>

#include <chrono>

TEST_CASE("Default Card has sensible defaults", "[card]") {
    fsrs::Card c;
    REQUIRE(c.state == fsrs::State::Learning);
    REQUIRE(c.step.has_value());
    REQUIRE(*c.step == 0);
    REQUIRE_FALSE(c.stability.has_value());
    REQUIRE_FALSE(c.difficulty.has_value());
    REQUIRE_FALSE(c.last_review.has_value());
    REQUIRE(c.card_id > 0);
}

TEST_CASE("Two default Cards get distinct card_ids", "[card]") {
    fsrs::Card a;
    fsrs::Card b;
    REQUIRE(a.card_id != b.card_id);
}

TEST_CASE("Card is copy-constructible and assignable", "[card]") {
    fsrs::Card original;
    original.stability  = 5.0;
    original.difficulty = 7.0;

    fsrs::Card copy = original;
    REQUIRE(copy.card_id    == original.card_id);
    REQUIRE(copy.stability  == original.stability);
    REQUIRE(copy.difficulty == original.difficulty);
}

TEST_CASE("Card accepts explicit field values", "[card]") {
    using TP = fsrs::Card::TimePoint;
    const TP due = std::chrono::sys_days{std::chrono::year{2025}/1/1};

    fsrs::Card c{
        .card_id     = 42,
        .state       = fsrs::State::Review,
        .step        = std::nullopt,
        .stability   = 10.0,
        .difficulty  = 5.0,
        .due         = due,
        .last_review = std::nullopt,
    };
    REQUIRE(c.card_id    == 42);
    REQUIRE(c.state      == fsrs::State::Review);
    REQUIRE_FALSE(c.step.has_value());
    REQUIRE(c.stability  == 10.0);
    REQUIRE(c.difficulty == 5.0);
    REQUIRE(c.due        == due);
}

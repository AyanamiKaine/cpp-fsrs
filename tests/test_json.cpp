#include <catch2/catch_all.hpp>
#include <fsrs/json.hpp>

#include <chrono>

using namespace std::chrono_literals;

TEST_CASE("Card round-trip preserves all fields", "[json][card]") {
    fsrs::Card c;
    c.state       = fsrs::State::Review;
    c.step        = std::nullopt;
    c.stability   = 12.345;
    c.difficulty  = 6.7;
    c.due         = std::chrono::sys_time<std::chrono::microseconds>{
        std::chrono::sys_days{std::chrono::year{2025}/3/14}
            + std::chrono::hours{15} + std::chrono::minutes{9} + std::chrono::seconds{26}};
    c.last_review = c.due - std::chrono::days{1};

    auto json = fsrs::to_json(c);
    auto back = fsrs::card_from_json(json);
    REQUIRE(back.has_value());
    REQUIRE(back->card_id    == c.card_id);
    REQUIRE(back->state      == c.state);
    REQUIRE(back->step       == c.step);
    REQUIRE(back->stability  == c.stability);
    REQUIRE(back->difficulty == c.difficulty);
    REQUIRE(back->due        == c.due);
    REQUIRE(back->last_review == c.last_review);
}

TEST_CASE("Card parses py-fsrs reference JSON", "[json][card]") {
    const char* src = R"({
        "card_id": 1, "state": 2, "step": null,
        "stability": 5.0, "difficulty": 4.0,
        "due": "2025-03-14T15:09:26+00:00",
        "last_review": null
    })";
    auto r = fsrs::card_from_json(src);
    REQUIRE(r.has_value());
    REQUIRE(r->card_id == 1);
    REQUIRE(r->state   == fsrs::State::Review);
    REQUIRE_FALSE(r->step.has_value());
    REQUIRE(r->stability.value()   == 5.0);
    REQUIRE(r->difficulty.value()  == 4.0);
    REQUIRE_FALSE(r->last_review.has_value());
}

TEST_CASE("ReviewLog round-trip", "[json][review_log]") {
    fsrs::ReviewLog l{
        .card_id = 42, .rating = fsrs::Rating::Hard,
        .review_datetime = std::chrono::sys_time<std::chrono::microseconds>{
            std::chrono::sys_days{std::chrono::year{2025}/1/1}},
        .review_duration = 1234ms,
    };
    auto back = fsrs::review_log_from_json(fsrs::to_json(l));
    REQUIRE(back.has_value());
    REQUIRE(back->card_id         == 42);
    REQUIRE(back->rating          == fsrs::Rating::Hard);
    REQUIRE(back->review_duration == 1234ms);
}

TEST_CASE("Scheduler round-trip", "[json][scheduler]") {
    fsrs::Scheduler::Config cfg;
    cfg.desired_retention = 0.85;
    cfg.maximum_interval  = 1000;
    cfg.enable_fuzzing    = false;
    fsrs::Scheduler s{cfg};

    auto json = fsrs::to_json(s);
    auto back = fsrs::scheduler_from_json(json);
    REQUIRE(back.has_value());
    REQUIRE(back->desired_retention() == 0.85);
    REQUIRE(back->maximum_interval()  == 1000);
    REQUIRE(back->enable_fuzzing()    == false);
}

TEST_CASE("card_from_json returns ParseError on malformed input", "[json][error]") {
    auto r = fsrs::card_from_json("{ not json");
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("card_from_json returns ParseError on missing field", "[json][error]") {
    auto r = fsrs::card_from_json(R"({"card_id": 1})");
    REQUIRE_FALSE(r.has_value());
}

TEST_CASE("scheduler_from_json surfaces validation errors as ParseError", "[json][error]") {
    const char* bad = R"({
        "parameters": [-1.0, 1.2931, 2.3065, 8.2956, 6.4133, 0.8334, 3.0194,
                       0.001, 1.8722, 0.1666, 0.796, 1.4835, 0.0614, 0.2629,
                       1.6483, 0.6014, 1.8729, 0.5425, 0.0912, 0.0658, 0.1542],
        "desired_retention": 0.9,
        "learning_steps": [60, 600],
        "relearning_steps": [600],
        "maximum_interval": 36500,
        "enable_fuzzing": true
    })";
    auto r = fsrs::scheduler_from_json(bad);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().message.find("parameters") != std::string::npos);
}

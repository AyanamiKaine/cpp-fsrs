#include <catch2/catch_all.hpp>
#include <fsrs/review_log.hpp>

#include <chrono>

TEST_CASE("ReviewLog holds fields", "[review_log]") {
    using namespace std::chrono_literals;
    const std::chrono::sys_time<std::chrono::microseconds> now =
        std::chrono::time_point_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now());

    fsrs::ReviewLog log{
        .card_id         = 42,
        .rating          = fsrs::Rating::Good,
        .review_datetime = now,
        .review_duration = 1500ms,
    };

    REQUIRE(log.card_id          == 42);
    REQUIRE(log.rating           == fsrs::Rating::Good);
    REQUIRE(log.review_datetime  == now);
    REQUIRE(log.review_duration.has_value());
    REQUIRE(*log.review_duration == 1500ms);
}

TEST_CASE("ReviewLog review_duration may be nullopt", "[review_log]") {
    fsrs::ReviewLog log{
        .card_id         = 1,
        .rating          = fsrs::Rating::Again,
        .review_datetime = {},
        .review_duration = std::nullopt,
    };
    REQUIRE_FALSE(log.review_duration.has_value());
}

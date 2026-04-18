#include <catch2/catch_all.hpp>
#include <fsrs/rating.hpp>
#include <fsrs/state.hpp>

#include <type_traits>

TEST_CASE("Rating enum has correct integer values", "[rating]") {
    STATIC_REQUIRE(static_cast<int>(fsrs::Rating::Again) == 1);
    STATIC_REQUIRE(static_cast<int>(fsrs::Rating::Hard)  == 2);
    STATIC_REQUIRE(static_cast<int>(fsrs::Rating::Good)  == 3);
    STATIC_REQUIRE(static_cast<int>(fsrs::Rating::Easy)  == 4);
    STATIC_REQUIRE(std::is_same_v<std::underlying_type_t<fsrs::Rating>, int>);
}

TEST_CASE("State enum has correct integer values", "[state]") {
    STATIC_REQUIRE(static_cast<int>(fsrs::State::Learning)   == 1);
    STATIC_REQUIRE(static_cast<int>(fsrs::State::Review)     == 2);
    STATIC_REQUIRE(static_cast<int>(fsrs::State::Relearning) == 3);
    STATIC_REQUIRE(std::is_same_v<std::underlying_type_t<fsrs::State>, int>);
}

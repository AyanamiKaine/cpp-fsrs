#include <catch2/catch_all.hpp>
#include <fsrs/scheduler.hpp>
#include <fsrs/error.hpp>

#include <chrono>

using namespace std::chrono_literals;

TEST_CASE("Default-constructed Scheduler works", "[validation]") {
    fsrs::Scheduler s;  // throwing ctor with DEFAULT_PARAMETERS
    REQUIRE(s.desired_retention() == 0.9);
    REQUIRE(s.maximum_interval()  == 36500);
    REQUIRE(s.enable_fuzzing()    == true);
    REQUIRE(s.learning_steps().size()   == 2);
    REQUIRE(s.relearning_steps().size() == 1);
    REQUIRE(s.parameters().size() == 21);
}

TEST_CASE("Scheduler::make accepts valid config", "[validation]") {
    fsrs::Scheduler::Config cfg{};
    auto r = fsrs::Scheduler::make(cfg);
    REQUIRE(r.has_value());
}

TEST_CASE("Scheduler::make rejects single out-of-bounds parameter", "[validation]") {
    fsrs::Scheduler::Config cfg{};
    cfg.parameters[0] = -1.0;  // LOWER bound is STABILITY_MIN = 0.001
    auto r = fsrs::Scheduler::make(cfg);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().details.size() == 1);
    REQUIRE(r.error().details[0].find("parameters[0]") != std::string::npos);
}

TEST_CASE("Scheduler::make accumulates ALL violations", "[validation]") {
    fsrs::Scheduler::Config cfg{};
    cfg.parameters[0] = -1.0;       // below lower
    cfg.parameters[4] = 999.0;      // above upper (10.0)
    cfg.parameters[20] = 0.0;       // below lower (0.1)
    auto r = fsrs::Scheduler::make(cfg);
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().details.size() == 3);
}

TEST_CASE("Throwing ctor throws ValidationError on bad config", "[validation]") {
    fsrs::Scheduler::Config cfg{};
    cfg.parameters[0] = -1.0;
    REQUIRE_THROWS_AS(fsrs::Scheduler(cfg), fsrs::ValidationError);
}

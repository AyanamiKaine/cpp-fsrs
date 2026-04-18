#include <catch2/catch_all.hpp>
#include <fsrs/scheduler.hpp>

#include <array>
#include <chrono>

using Catch::Approx;
using namespace std::chrono_literals;

// Port of py-fsrs tests/test_basic.py::TestPyFSRS::test_review_card
// Golden interval history:  [0, 2, 11, 46, 163, 498, 0, 0, 2, 4, 7, 12, 21]
TEST_CASE("test_review_card reproduces py-fsrs interval history", "[basic][oracle]") {
    fsrs::Scheduler::Config cfg;
    cfg.enable_fuzzing = false;
    fsrs::Scheduler s{cfg};

    const std::array ratings = {
        fsrs::Rating::Good,  fsrs::Rating::Good,  fsrs::Rating::Good,
        fsrs::Rating::Good,  fsrs::Rating::Good,  fsrs::Rating::Good,
        fsrs::Rating::Again, fsrs::Rating::Again, fsrs::Rating::Good,
        fsrs::Rating::Good,  fsrs::Rating::Good,  fsrs::Rating::Good,
        fsrs::Rating::Good,
    };
    const std::array expected = {0, 2, 11, 46, 163, 498, 0, 0, 2, 4, 7, 12, 21};

    fsrs::Card c;
    auto when = std::chrono::sys_time<std::chrono::microseconds>{
        std::chrono::sys_days{std::chrono::year{2022}/11/29}
            + std::chrono::hours{12} + std::chrono::minutes{30}};

    std::vector<long long> history;
    for (auto r : ratings) {
        auto [next, _] = s.review_card(c, r, when);
        c    = next;
        when = c.due;
        const auto dur = c.due - *c.last_review;
        history.push_back(std::chrono::duration_cast<std::chrono::days>(dur).count());
    }
    REQUIRE(history.size() == expected.size());
    for (std::size_t i = 0; i < expected.size(); ++i) {
        CAPTURE(i, history[i], expected[i]);
        REQUIRE(history[i] == expected[i]);
    }
}

// Port of test_basic.py::test_repeated_correct_reviews
TEST_CASE("Repeated Easy in Learning drives difficulty to 1.0", "[basic][oracle]") {
    fsrs::Scheduler::Config cfg; cfg.enable_fuzzing = false;
    fsrs::Scheduler s{cfg};

    fsrs::Card c;
    auto base = std::chrono::sys_time<std::chrono::microseconds>{
        std::chrono::sys_days{std::chrono::year{2022}/11/29}
            + std::chrono::hours{12} + std::chrono::minutes{30}};

    for (int i = 0; i < 10; ++i) {
        auto [next, _] = s.review_card(c, fsrs::Rating::Easy, base + std::chrono::seconds{i});
        c = next;
    }
    REQUIRE(c.difficulty.has_value());
    REQUIRE(*c.difficulty == Approx(1.0));
}

// Port of test_basic.py::test_memo_state
TEST_CASE("Memo-state stability and difficulty after 6 reviews", "[basic][oracle]") {
    fsrs::Scheduler s;  // fuzzing ON per py-fsrs, but fuzzing does not affect stability/difficulty

    const std::array ratings = {
        fsrs::Rating::Again, fsrs::Rating::Good, fsrs::Rating::Good,
        fsrs::Rating::Good,  fsrs::Rating::Good, fsrs::Rating::Good,
    };
    const std::array<int,6> ivls = {0, 0, 1, 3, 8, 21};

    fsrs::Card c;
    auto when = std::chrono::sys_time<std::chrono::microseconds>{
        std::chrono::sys_days{std::chrono::year{2022}/11/29}
            + std::chrono::hours{12} + std::chrono::minutes{30}};

    for (std::size_t i = 0; i < ratings.size(); ++i) {
        when = when + std::chrono::days{ivls[i]};
        auto [next, _] = s.review_card(c, ratings[i], when);
        c = next;
    }

    REQUIRE(c.stability.has_value());
    REQUIRE(c.difficulty.has_value());
    REQUIRE(*c.stability  == Approx(53.62691).margin(1e-4));
    REQUIRE(*c.difficulty == Approx(6.3574867).margin(1e-4));
}

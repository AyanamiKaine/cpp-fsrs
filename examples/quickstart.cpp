// Port of the py-fsrs README quickstart into C++23.
#include <fsrs/fsrs.hpp>

#include <chrono>
#include <iostream>
#include <print>

int main() {
    fsrs::Scheduler scheduler;
    fsrs::Card      card;

    auto [reviewed, log] = scheduler.review_card(card, fsrs::Rating::Good);

    std::println("Card rated {} at {}",
                 static_cast<int>(log.rating),
                 log.review_datetime);

    const auto now = std::chrono::time_point_cast<std::chrono::microseconds>(
        std::chrono::system_clock::now());
    const auto delta = reviewed.due - now;
    std::println("Card due at {}", reviewed.due);
    std::println("Card due in {} seconds",
                 std::chrono::duration_cast<std::chrono::seconds>(delta).count());
    return 0;
}

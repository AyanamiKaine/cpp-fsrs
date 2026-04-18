#pragma once

#include <fsrs/state.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <optional>

namespace fsrs {

struct Card {
    using TimePoint = std::chrono::sys_time<std::chrono::microseconds>;

    static inline std::int64_t next_fresh_id() {
        const auto nowUs = std::chrono::time_point_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now());
        static std::atomic<std::int64_t> counter{0};
        return nowUs.time_since_epoch().count() * 1024
             + (counter.fetch_add(1, std::memory_order_relaxed) & 0x3FF);
    }
    static inline TimePoint now_us() {
        return std::chrono::time_point_cast<std::chrono::microseconds>(
            std::chrono::system_clock::now());
    }

    std::int64_t            card_id     = next_fresh_id();
    State                   state       = State::Learning;
    std::optional<int>      step        = 0;
    std::optional<double>   stability;
    std::optional<double>   difficulty;
    TimePoint               due         = now_us();
    std::optional<TimePoint> last_review;
};

}  // namespace fsrs

#pragma once

#include <fsrs/rating.hpp>

#include <chrono>
#include <cstdint>
#include <optional>

namespace fsrs {

struct ReviewLog {
    using TimePoint = std::chrono::sys_time<std::chrono::microseconds>;

    std::int64_t                          card_id;
    Rating                                rating;
    TimePoint                             review_datetime;
    std::optional<std::chrono::milliseconds> review_duration;
};

}  // namespace fsrs

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>
#include <utility>

namespace fsrs::detail {

struct FuzzRange {
    double start;
    double end;
    double factor;
};

inline constexpr std::array<FuzzRange, 3> FUZZ_RANGES{{
    {2.5,  7.0,  0.15},
    {7.0,  20.0, 0.10},
    {20.0, std::numeric_limits<double>::infinity(), 0.05},
}};

// Returns the [min_ivl, max_ivl] fuzz window for a given interval in days.
inline std::pair<int,int> fuzz_window(int interval_days, int maximum_interval) {
    double delta = 1.0;
    for (const auto& r : FUZZ_RANGES) {
        delta += r.factor * std::max(std::min<double>(interval_days, r.end) - r.start, 0.0);
    }
    int min_ivl = static_cast<int>(std::llround(interval_days - delta));
    int max_ivl = static_cast<int>(std::llround(interval_days + delta));
    min_ivl = std::max(2, min_ivl);
    max_ivl = std::min(max_ivl, maximum_interval);
    min_ivl = std::min(min_ivl, max_ivl);
    return {min_ivl, max_ivl};
}

template <std::uniform_random_bit_generator G>
inline int get_fuzzed_interval(int interval_days, G& rng, int maximum_interval) {
    if (interval_days < 3) {  // py-fsrs: "< 2.5" on the underlying double
        return interval_days;
    }
    auto [lo, hi] = fuzz_window(interval_days, maximum_interval);
    std::uniform_int_distribution<int> dist(lo, hi);
    int v = dist(rng);
    return std::min(v, maximum_interval);
}

}  // namespace fsrs::detail

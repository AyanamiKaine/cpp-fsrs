#pragma once

#include <array>

namespace fsrs::detail {

inline constexpr double STABILITY_MIN         = 0.001;
inline constexpr double INITIAL_STABILITY_MAX = 100.0;
inline constexpr double MIN_DIFFICULTY        = 1.0;
inline constexpr double MAX_DIFFICULTY        = 10.0;
inline constexpr double FSRS_DEFAULT_DECAY    = 0.1542;

inline constexpr std::array<double, 21> DEFAULT_PARAMETERS{
    0.212, 1.2931, 2.3065, 8.2956, 6.4133, 0.8334, 3.0194,
    0.001, 1.8722, 0.1666, 0.796,  1.4835, 0.0614, 0.2629,
    1.6483, 0.6014, 1.8729, 0.5425, 0.0912, 0.0658, FSRS_DEFAULT_DECAY,
};

inline constexpr std::array<double, 21> LOWER_BOUNDS_PARAMETERS{
    STABILITY_MIN, STABILITY_MIN, STABILITY_MIN, STABILITY_MIN,
    1.0, 0.001, 0.001, 0.001, 0.0, 0.0, 0.001,
    0.001, 0.001, 0.001, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.1,
};

inline constexpr std::array<double, 21> UPPER_BOUNDS_PARAMETERS{
    INITIAL_STABILITY_MAX, INITIAL_STABILITY_MAX,
    INITIAL_STABILITY_MAX, INITIAL_STABILITY_MAX,
    10.0, 4.0, 4.0, 0.75, 4.5, 0.8, 3.5,
    5.0, 0.25, 0.9, 4.0, 1.0, 6.0, 2.0, 2.0, 0.8, 0.8,
};

}  // namespace fsrs::detail

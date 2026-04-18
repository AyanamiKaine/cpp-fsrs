#pragma once

#include <fsrs/card.hpp>
#include <fsrs/detail/fuzz.hpp>
#include <fsrs/detail/math.hpp>
#include <fsrs/error.hpp>
#include <fsrs/rating.hpp>
#include <fsrs/review_log.hpp>
#include <fsrs/state.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <expected>
#include <format>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace fsrs {

class Scheduler {
public:
    using Duration   = std::chrono::milliseconds;
    using TimePoint  = std::chrono::sys_time<std::chrono::microseconds>;
    using Parameters = std::array<double, 21>;

    static constexpr Parameters DEFAULT_PARAMETERS = detail::DEFAULT_PARAMETERS;

    struct Config {
        Parameters            parameters        = DEFAULT_PARAMETERS;
        double                desired_retention = 0.9;
        std::vector<Duration> learning_steps    = {
            std::chrono::duration_cast<Duration>(std::chrono::minutes{1}),
            std::chrono::duration_cast<Duration>(std::chrono::minutes{10}),
        };
        std::vector<Duration> relearning_steps  = {
            std::chrono::duration_cast<Duration>(std::chrono::minutes{10}),
        };
        int                   maximum_interval  = 36500;
        bool                  enable_fuzzing    = true;
    };

    // Throwing ctor — invalid config throws ValidationError.
    Scheduler() : Scheduler(Config{}) {}
    explicit Scheduler(Config cfg) {
        auto r = make(std::move(cfg));
        if (!r) {
            throw std::move(r.error());
        }
        *this = std::move(*r);
    }

    static std::expected<Scheduler, ValidationError> make(Config cfg) {
        ValidationError err;
        for (std::size_t i = 0; i < cfg.parameters.size(); ++i) {
            const double p  = cfg.parameters[i];
            const double lo = detail::LOWER_BOUNDS_PARAMETERS[i];
            const double hi = detail::UPPER_BOUNDS_PARAMETERS[i];
            if (!(lo <= p && p <= hi)) {
                err.details.push_back(std::format(
                    "parameters[{}] = {} is out of bounds: ({}, {})", i, p, lo, hi));
            }
        }
        if (!err.details.empty()) {
            err.message = "One or more parameters are out of bounds";
            return std::unexpected(std::move(err));
        }
        return Scheduler{InternalCtor{}, std::move(cfg)};
    }

    // Accessors
    const Parameters& parameters()        const noexcept { return parameters_; }
    double            desired_retention() const noexcept { return desired_retention_; }
    std::span<const Duration> learning_steps()   const noexcept { return learning_steps_; }
    std::span<const Duration> relearning_steps() const noexcept { return relearning_steps_; }
    int               maximum_interval()  const noexcept { return maximum_interval_; }
    bool              enable_fuzzing()    const noexcept { return enable_fuzzing_; }

    double decay()  const noexcept { return decay_; }
    double factor() const noexcept { return factor_; }

    double get_card_retrievability(const Card& card,
                                   std::optional<TimePoint> when = std::nullopt) const {
        if (!card.last_review || !card.stability) return 0.0;
        if (!when) {
            when = std::chrono::time_point_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now());
        }
        const auto diff = *when - *card.last_review;
        long long elapsed_days =
            std::chrono::duration_cast<std::chrono::days>(diff).count();
        if (elapsed_days < 0) elapsed_days = 0;  // matches py-fsrs max(0, ...)
        return std::pow(1.0 + factor_ * static_cast<double>(elapsed_days) / *card.stability,
                        decay_);
    }

    // Default RNG overload: thread-local mt19937 seeded once.
    std::pair<Card, ReviewLog> review_card(
        Card card, Rating rating,
        std::optional<TimePoint> when = std::nullopt,
        std::optional<Duration> review_duration = std::nullopt) const
    {
        static thread_local std::mt19937 rng{std::random_device{}()};
        return review_card(std::move(card), rating, when, review_duration, rng);
    }

    template <std::uniform_random_bit_generator G>
    std::pair<Card, ReviewLog> review_card(
        Card card, Rating rating,
        std::optional<TimePoint> when,
        std::optional<Duration> review_duration,
        G& rng) const
    {
        if (!when) {
            when = std::chrono::time_point_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now());
        }

        std::optional<long long> days_since_last_review;
        if (card.last_review) {
            const auto diff = *when - *card.last_review;
            days_since_last_review = std::chrono::duration_cast<std::chrono::days>(diff).count();
        }

        Duration next_ivl{};
        switch (card.state) {
            case State::Learning:
                next_ivl = handle_learning(card, rating, *when, days_since_last_review);
                break;
            case State::Review:
                next_ivl = handle_review(card, rating, *when, days_since_last_review);
                break;
            case State::Relearning:
                next_ivl = handle_relearning(card, rating, *when, days_since_last_review);
                break;
        }

        if (enable_fuzzing_ && card.state == State::Review) {
            auto fuzz_days = std::chrono::duration_cast<std::chrono::days>(next_ivl).count();
            int fuzzed = detail::get_fuzzed_interval(
                static_cast<int>(fuzz_days), rng, maximum_interval_);
            next_ivl = std::chrono::duration_cast<Duration>(std::chrono::days{fuzzed});
        }

        card.due         = *when + next_ivl;
        card.last_review = *when;

        ReviewLog log{
            .card_id         = card.card_id,
            .rating          = rating,
            .review_datetime = *when,
            .review_duration = review_duration,
        };
        return {std::move(card), std::move(log)};
    }

    Card reschedule_card(Card card, std::vector<ReviewLog> logs) const {
        for (const auto& l : logs) {
            if (l.card_id != card.card_id) {
                throw std::invalid_argument(std::format(
                    "ReviewLog card_id {} does not match Card card_id {}",
                    l.card_id, card.card_id));
            }
        }
        std::sort(logs.begin(), logs.end(), [](const auto& a, const auto& b) {
            return a.review_datetime < b.review_datetime;
        });

        Card out;
        out.card_id = card.card_id;
        out.due     = card.due;
        for (const auto& l : logs) {
            auto [next, _] = review_card(out, l.rating, l.review_datetime);
            out = std::move(next);
        }
        return out;
    }

    // ---- Numeric helpers (public for testability; not part of the curated API) ----

    double clamp_difficulty(double d) const noexcept {
        return std::min(std::max(d, detail::MIN_DIFFICULTY), detail::MAX_DIFFICULTY);
    }

    double clamp_stability(double s) const noexcept {
        return std::max(s, detail::STABILITY_MIN);
    }

    double initial_stability(Rating rating) const noexcept {
        return clamp_stability(parameters_[static_cast<int>(rating) - 1]);
    }

    double initial_difficulty(Rating rating, bool clamp = true) const noexcept {
        const double r = static_cast<double>(static_cast<int>(rating));
        double d = parameters_[4] - std::exp(parameters_[5] * (r - 1.0)) + 1.0;
        return clamp ? clamp_difficulty(d) : d;
    }

    int next_interval(double stability) const noexcept {
        double raw = (stability / factor_) *
                     (std::pow(desired_retention_, 1.0 / decay_) - 1.0);
        long long rounded = static_cast<long long>(std::llround(raw));
        if (rounded < 1)                 rounded = 1;
        if (rounded > maximum_interval_) rounded = maximum_interval_;
        return static_cast<int>(rounded);
    }

    double short_term_stability(double stability, Rating rating) const noexcept {
        const double r  = static_cast<double>(static_cast<int>(rating));
        double increase = std::exp(parameters_[17] * (r - 3.0 + parameters_[18]))
                        * std::pow(stability, -parameters_[19]);
        if (rating == Rating::Good || rating == Rating::Easy) {
            increase = std::max(increase, 1.0);
        }
        return clamp_stability(stability * increase);
    }

    double next_difficulty(double difficulty, Rating rating) const noexcept {
        const double r = static_cast<double>(static_cast<int>(rating));
        const double delta       = -(parameters_[6] * (r - 3.0));
        const double linear_damp = (10.0 - difficulty) * delta / 9.0;
        const double arg2        = difficulty + linear_damp;
        const double arg1        = initial_difficulty(Rating::Easy, /*clamp=*/false);
        const double mean_rev    = parameters_[7] * arg1 + (1.0 - parameters_[7]) * arg2;
        return clamp_difficulty(mean_rev);
    }

    double next_forget_stability(double difficulty, double stability, double retrievability) const noexcept {
        const double long_term = parameters_[11]
            * std::pow(difficulty, -parameters_[12])
            * (std::pow(stability + 1.0, parameters_[13]) - 1.0)
            * std::exp((1.0 - retrievability) * parameters_[14]);
        const double short_term = stability / std::exp(parameters_[17] * parameters_[18]);
        return std::min(long_term, short_term);
    }

    double next_recall_stability(double difficulty, double stability, double retrievability, Rating rating) const noexcept {
        const double hard_penalty = (rating == Rating::Hard) ? parameters_[15] : 1.0;
        const double easy_bonus   = (rating == Rating::Easy) ? parameters_[16] : 1.0;
        return stability * (1.0
            + std::exp(parameters_[8])
            * (11.0 - difficulty)
            * std::pow(stability, -parameters_[9])
            * (std::exp((1.0 - retrievability) * parameters_[10]) - 1.0)
            * hard_penalty
            * easy_bonus);
    }

    double next_stability(double difficulty, double stability, double retrievability, Rating rating) const noexcept {
        double s = (rating == Rating::Again)
            ? next_forget_stability(difficulty, stability, retrievability)
            : next_recall_stability(difficulty, stability, retrievability, rating);
        return clamp_stability(s);
    }

private:
    Duration handle_learning(Card& card, Rating rating, TimePoint when,
                             std::optional<long long> days_since) const {
        // Update stability / difficulty
        if (!card.stability || !card.difficulty) {
            card.stability  = initial_stability(rating);
            card.difficulty = initial_difficulty(rating, /*clamp=*/true);
        } else if (days_since && *days_since < 1) {
            card.stability  = short_term_stability(*card.stability, rating);
            card.difficulty = next_difficulty(*card.difficulty, rating);
        } else {
            const double r = get_card_retrievability(card, when);
            card.stability  = next_stability(*card.difficulty, *card.stability, r, rating);
            card.difficulty = next_difficulty(*card.difficulty, rating);
        }

        const auto n_steps = static_cast<int>(learning_steps_.size());
        if (n_steps == 0
            || (card.step && *card.step >= n_steps
                && rating != Rating::Again)) {
            card.state = State::Review;
            card.step.reset();
            return std::chrono::duration_cast<Duration>(
                std::chrono::days{next_interval(*card.stability)});
        }

        const int step = card.step.value_or(0);
        switch (rating) {
            case Rating::Again:
                card.step = 0;
                return learning_steps_[0];
            case Rating::Hard:
                if (step == 0 && n_steps == 1) return learning_steps_[0] * 3 / 2;
                if (step == 0 && n_steps >= 2) return (learning_steps_[0] + learning_steps_[1]) / 2;
                return learning_steps_[step];
            case Rating::Good:
                if (step + 1 == n_steps) {
                    card.state = State::Review;
                    card.step.reset();
                    return std::chrono::duration_cast<Duration>(
                        std::chrono::days{next_interval(*card.stability)});
                }
                card.step = step + 1;
                return learning_steps_[step + 1];
            case Rating::Easy:
                card.state = State::Review;
                card.step.reset();
                return std::chrono::duration_cast<Duration>(
                    std::chrono::days{next_interval(*card.stability)});
        }
        assert(false && "unknown Rating");
        throw std::invalid_argument("unknown Rating");
    }

    Duration handle_review(Card& card, Rating rating, TimePoint when,
                           std::optional<long long> days_since) const {
        // Stability / difficulty
        if (days_since && *days_since < 1) {
            card.stability = short_term_stability(*card.stability, rating);
        } else {
            const double r = get_card_retrievability(card, when);
            card.stability = next_stability(*card.difficulty, *card.stability, r, rating);
        }
        card.difficulty = next_difficulty(*card.difficulty, rating);

        // Transition
        switch (rating) {
            case Rating::Again:
                if (relearning_steps_.empty()) {
                    return std::chrono::duration_cast<Duration>(
                        std::chrono::days{next_interval(*card.stability)});
                }
                card.state = State::Relearning;
                card.step  = 0;
                return relearning_steps_[0];

            case Rating::Hard:
            case Rating::Good:
            case Rating::Easy:
                return std::chrono::duration_cast<Duration>(
                    std::chrono::days{next_interval(*card.stability)});
        }
        assert(false && "unknown Rating");
        throw std::invalid_argument("unknown Rating");
    }
    Duration handle_relearning(Card& card, Rating rating, TimePoint when,
                               std::optional<long long> days_since) const {
        if (days_since && *days_since < 1) {
            card.stability  = short_term_stability(*card.stability, rating);
            card.difficulty = next_difficulty(*card.difficulty, rating);
        } else {
            const double r = get_card_retrievability(card, when);
            card.stability  = next_stability(*card.difficulty, *card.stability, r, rating);
            card.difficulty = next_difficulty(*card.difficulty, rating);
        }

        const auto n_steps = static_cast<int>(relearning_steps_.size());
        if (n_steps == 0
            || (card.step && *card.step >= n_steps && rating != Rating::Again)) {
            card.state = State::Review;
            card.step.reset();
            return std::chrono::duration_cast<Duration>(
                std::chrono::days{next_interval(*card.stability)});
        }

        const int step = card.step.value_or(0);
        switch (rating) {
            case Rating::Again:
                card.step = 0;
                return relearning_steps_[0];
            case Rating::Hard:
                if (step == 0 && n_steps == 1) return relearning_steps_[0] * 3 / 2;
                if (step == 0 && n_steps >= 2) return (relearning_steps_[0] + relearning_steps_[1]) / 2;
                return relearning_steps_[step];
            case Rating::Good:
                if (step + 1 == n_steps) {
                    card.state = State::Review;
                    card.step.reset();
                    return std::chrono::duration_cast<Duration>(
                        std::chrono::days{next_interval(*card.stability)});
                }
                card.step = step + 1;
                return relearning_steps_[step + 1];
            case Rating::Easy:
                card.state = State::Review;
                card.step.reset();
                return std::chrono::duration_cast<Duration>(
                    std::chrono::days{next_interval(*card.stability)});
        }
        assert(false && "unknown Rating");
        throw std::invalid_argument("unknown Rating");
    }

    struct InternalCtor {};
    Scheduler(InternalCtor, Config cfg)
        : parameters_(cfg.parameters)
        , desired_retention_(cfg.desired_retention)
        , learning_steps_(std::move(cfg.learning_steps))
        , relearning_steps_(std::move(cfg.relearning_steps))
        , maximum_interval_(cfg.maximum_interval)
        , enable_fuzzing_(cfg.enable_fuzzing)
        , decay_(-parameters_[20])
        , factor_(std::pow(0.9, 1.0 / decay_) - 1.0)
    {}

    Parameters            parameters_{};
    double                desired_retention_ = 0.9;
    std::vector<Duration> learning_steps_;
    std::vector<Duration> relearning_steps_;
    int                   maximum_interval_  = 36500;
    bool                  enable_fuzzing_    = true;
    double                decay_             = -detail::FSRS_DEFAULT_DECAY;
    double                factor_            = 0.0;
};

}  // namespace fsrs

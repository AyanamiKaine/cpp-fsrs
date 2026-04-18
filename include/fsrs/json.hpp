#pragma once

#include <fsrs/card.hpp>
#include <fsrs/error.hpp>
#include <fsrs/review_log.hpp>
#include <fsrs/scheduler.hpp>

#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdint>
#include <expected>
#include <format>
#include <sstream>
#include <string>
#include <string_view>

namespace fsrs {
namespace detail {

// Serialize sys_time<microseconds> in py-fsrs-compatible ISO-8601 form.
// Output: "YYYY-MM-DDTHH:MM:SS+00:00" (no fractional) or "YYYY-MM-DDTHH:MM:SS.ffffff+00:00".
inline std::string iso8601(Card::TimePoint tp) {
    using namespace std::chrono;
    const auto secs = floor<seconds>(tp);
    const auto us   = duration_cast<microseconds>(tp - secs).count();
    const auto ymd  = year_month_day{floor<days>(secs)};
    const auto tod  = hh_mm_ss{secs - floor<days>(secs)};
    if (us == 0) {
        return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}+00:00",
            static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month()),
            static_cast<unsigned>(ymd.day()),
            tod.hours().count(), tod.minutes().count(),
            static_cast<long long>(tod.seconds().count()));
    }
    return std::format("{:04}-{:02}-{:02}T{:02}:{:02}:{:02}.{:06}+00:00",
        static_cast<int>(ymd.year()), static_cast<unsigned>(ymd.month()),
        static_cast<unsigned>(ymd.day()),
        tod.hours().count(), tod.minutes().count(),
        static_cast<long long>(tod.seconds().count()), us);
}

// Parse ISO-8601 with optional fractional seconds and UTC offset "+00:00" or "Z".
// Throws std::runtime_error on malformed input.
inline Card::TimePoint parse_iso8601(std::string_view s) {
    std::string buf{s};
    if (!buf.empty() && buf.back() == 'Z') {
        buf.pop_back();
        buf += "+00:00";
    }
    std::istringstream in{buf};
    Card::TimePoint tp;
    in >> std::chrono::parse("%FT%T%Ez", tp);
    if (in.fail()) {
        throw std::runtime_error(std::format("invalid ISO-8601 datetime: '{}'", s));
    }
    return tp;
}

}  // namespace detail

// ---- Card ----

inline std::string to_json(const Card& c, int indent = -1) {
    nlohmann::json j = {
        {"card_id",     c.card_id},
        {"state",       static_cast<int>(c.state)},
        {"step",        c.step.has_value() ? nlohmann::json(*c.step) : nlohmann::json(nullptr)},
        {"stability",   c.stability.has_value()  ? nlohmann::json(*c.stability)  : nlohmann::json(nullptr)},
        {"difficulty",  c.difficulty.has_value() ? nlohmann::json(*c.difficulty) : nlohmann::json(nullptr)},
        {"due",         detail::iso8601(c.due)},
        {"last_review", c.last_review.has_value() ? nlohmann::json(detail::iso8601(*c.last_review))
                                                  : nlohmann::json(nullptr)},
    };
    return j.dump(indent);
}

inline std::expected<Card, ParseError> card_from_json(std::string_view src) {
    try {
        auto j = nlohmann::json::parse(src);
        Card c;
        c.card_id = j.at("card_id").get<std::int64_t>();
        c.state   = static_cast<State>(j.at("state").get<int>());
        if (j.at("step").is_null())       c.step.reset();       else c.step       = j["step"].get<int>();
        if (j.at("stability").is_null())  c.stability.reset();  else c.stability  = j["stability"].get<double>();
        if (j.at("difficulty").is_null()) c.difficulty.reset(); else c.difficulty = j["difficulty"].get<double>();
        c.due = detail::parse_iso8601(j.at("due").get<std::string>());
        if (j.at("last_review").is_null()) c.last_review.reset();
        else                               c.last_review = detail::parse_iso8601(j["last_review"].get<std::string>());
        return c;
    } catch (const std::exception& e) {
        return std::unexpected(ParseError{e.what()});
    }
}

// ---- ReviewLog ----

inline std::string to_json(const ReviewLog& l, int indent = -1) {
    nlohmann::json j = {
        {"card_id",         l.card_id},
        {"rating",          static_cast<int>(l.rating)},
        {"review_datetime", detail::iso8601(l.review_datetime)},
        {"review_duration", l.review_duration.has_value()
                                ? nlohmann::json(l.review_duration->count())
                                : nlohmann::json(nullptr)},
    };
    return j.dump(indent);
}

inline std::expected<ReviewLog, ParseError> review_log_from_json(std::string_view src) {
    try {
        auto j = nlohmann::json::parse(src);
        ReviewLog l;
        l.card_id         = j.at("card_id").get<std::int64_t>();
        l.rating          = static_cast<Rating>(j.at("rating").get<int>());
        l.review_datetime = detail::parse_iso8601(j.at("review_datetime").get<std::string>());
        if (j.at("review_duration").is_null()) l.review_duration.reset();
        else l.review_duration = std::chrono::milliseconds{j["review_duration"].get<long long>()};
        return l;
    } catch (const std::exception& e) {
        return std::unexpected(ParseError{e.what()});
    }
}

// ---- Scheduler ----

inline std::string to_json(const Scheduler& s, int indent = -1) {
    nlohmann::json params = nlohmann::json::array();
    for (double p : s.parameters()) params.push_back(p);

    nlohmann::json ls = nlohmann::json::array();
    for (auto d : s.learning_steps())
        ls.push_back(std::chrono::duration_cast<std::chrono::seconds>(d).count());

    nlohmann::json rs = nlohmann::json::array();
    for (auto d : s.relearning_steps())
        rs.push_back(std::chrono::duration_cast<std::chrono::seconds>(d).count());

    nlohmann::json j = {
        {"parameters",        params},
        {"desired_retention", s.desired_retention()},
        {"learning_steps",    ls},
        {"relearning_steps",  rs},
        {"maximum_interval",  s.maximum_interval()},
        {"enable_fuzzing",    s.enable_fuzzing()},
    };
    return j.dump(indent);
}

inline std::expected<Scheduler, ParseError> scheduler_from_json(std::string_view src) {
    try {
        auto j = nlohmann::json::parse(src);
        Scheduler::Config cfg;
        auto params_json = j.at("parameters");
        if (params_json.size() != cfg.parameters.size()) {
            return std::unexpected(ParseError{std::format(
                "expected {} parameters, got {}", cfg.parameters.size(), params_json.size())});
        }
        for (std::size_t i = 0; i < cfg.parameters.size(); ++i) {
            cfg.parameters[i] = params_json[i].get<double>();
        }
        cfg.desired_retention = j.at("desired_retention").get<double>();
        cfg.learning_steps.clear();
        for (const auto& x : j.at("learning_steps")) {
            cfg.learning_steps.push_back(std::chrono::duration_cast<Scheduler::Duration>(
                std::chrono::seconds{x.get<long long>()}));
        }
        cfg.relearning_steps.clear();
        for (const auto& x : j.at("relearning_steps")) {
            cfg.relearning_steps.push_back(std::chrono::duration_cast<Scheduler::Duration>(
                std::chrono::seconds{x.get<long long>()}));
        }
        cfg.maximum_interval = j.at("maximum_interval").get<int>();
        cfg.enable_fuzzing   = j.at("enable_fuzzing").get<bool>();

        auto r = Scheduler::make(std::move(cfg));
        if (!r) return std::unexpected(ParseError{r.error().message});
        return std::move(*r);
    } catch (const std::exception& e) {
        return std::unexpected(ParseError{e.what()});
    }
}

}  // namespace fsrs

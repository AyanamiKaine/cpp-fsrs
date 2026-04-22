#pragma once

#include <fsrs/card.hpp>
#include <fsrs/error.hpp>
#include <fsrs/review_log.hpp>
#include <fsrs/scheduler.hpp>

#include <nlohmann/json.hpp>

#include <cctype>
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>

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

// Parse ISO-8601 with optional fractional seconds and a numeric UTC offset
// of the form "+HH:MM" / "-HH:MM", or the shorthand "Z" (= "+00:00").
// Implemented by hand because std::chrono::parse is only available in very
// recent libc++ versions (libc++ 20+), which excludes Apple Clang on macOS.
// Throws std::runtime_error on malformed input.
inline Card::TimePoint parse_iso8601(std::string_view s) {
    using namespace std::chrono;

    auto bad = [&] {
        return std::runtime_error(std::format("invalid ISO-8601 datetime: '{}'", s));
    };

    // Normalize a trailing 'Z' to "+00:00" so the offset parser has one shape.
    std::string buf{s};
    if (!buf.empty() && buf.back() == 'Z') {
        buf.pop_back();
        buf += "+00:00";
    }

    // Fixed-width unsigned integer parse from a known position.
    auto parse_uint = [](const char* p, std::size_t width, int& out) {
        auto [ptr, ec] = std::from_chars(p, p + width, out);
        return ec == std::errc{} && ptr == p + width;
    };

    // Minimum legal length: "YYYY-MM-DDTHH:MM:SS" (19) + "+HH:MM" (6) = 25.
    if (buf.size() < 25) throw bad();

    int y, mo, d, h, mi, se;
    const char* p = buf.data();
    if (!parse_uint(p,      4, y)  || p[4]  != '-' ||
        !parse_uint(p +  5, 2, mo) || p[7]  != '-' ||
        !parse_uint(p +  8, 2, d)  || p[10] != 'T' ||
        !parse_uint(p + 11, 2, h)  || p[13] != ':' ||
        !parse_uint(p + 14, 2, mi) || p[16] != ':' ||
        !parse_uint(p + 17, 2, se)) {
        throw bad();
    }

    // Optional ".f..." fractional seconds. Pad/truncate to microseconds (6 digits).
    std::size_t pos = 19;
    long long frac_us = 0;
    if (pos < buf.size() && buf[pos] == '.') {
        ++pos;
        const std::size_t start = pos;
        while (pos < buf.size() && std::isdigit(static_cast<unsigned char>(buf[pos]))) ++pos;
        const std::size_t n = pos - start;
        if (n == 0) throw bad();
        char digits[7] = {'0','0','0','0','0','0','\0'};
        for (std::size_t i = 0; i < n && i < 6; ++i) digits[i] = buf[start + i];
        auto [_, ec] = std::from_chars(digits, digits + 6, frac_us);
        if (ec != std::errc{}) throw bad();
    }

    // Offset must be exactly the trailing 6 chars: (+|-)HH:MM
    if (pos + 6 != buf.size()) throw bad();
    const char sign = buf[pos];
    if (sign != '+' && sign != '-') throw bad();
    int oh, om;
    if (!parse_uint(buf.data() + pos + 1, 2, oh) || buf[pos + 3] != ':' ||
        !parse_uint(buf.data() + pos + 4, 2, om)) {
        throw bad();
    }
    if (oh > 23 || om > 59) throw bad();

    const year_month_day ymd{year{y}, month{static_cast<unsigned>(mo)},
                             day{static_cast<unsigned>(d)}};
    if (!ymd.ok()) throw bad();
    if (h > 23 || mi > 59 || se > 59) throw bad();

    // Wall-clock time with the offset still attached, then subtract the
    // offset to get UTC. "+05:00" means local is 5h ahead of UTC.
    Card::TimePoint local = sys_days{ymd} + hours{h} + minutes{mi}
                          + seconds{se} + microseconds{frac_us};
    auto offset = hours{oh} + minutes{om};
    if (sign == '-') offset = -offset;
    return local - offset;
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

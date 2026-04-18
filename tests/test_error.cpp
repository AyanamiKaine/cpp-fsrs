#include <catch2/catch_all.hpp>
#include <fsrs/error.hpp>

#include <expected>
#include <string>

TEST_CASE("ValidationError stores message and details", "[error]") {
    fsrs::ValidationError err{"bad params", {"parameters[0] = -1 out of bounds"}};
    REQUIRE(err.message == "bad params");
    REQUIRE(err.details.size() == 1);
    REQUIRE(std::string{err.what()} == "bad params");
}

TEST_CASE("ParseError stores message", "[error]") {
    fsrs::ParseError err{"unexpected token"};
    REQUIRE(err.message == "unexpected token");
    REQUIRE(std::string{err.what()} == "unexpected token");
}

TEST_CASE("ValidationError is catchable as std::exception", "[error]") {
    try {
        throw fsrs::ValidationError{"oops", {}};
    } catch (const std::exception& e) {
        REQUIRE(std::string{e.what()} == "oops");
    }
}

TEST_CASE("ValidationError usable as std::expected error", "[error]") {
    std::expected<int, fsrs::ValidationError> r =
        std::unexpected(fsrs::ValidationError{"v", {}});
    REQUIRE_FALSE(r.has_value());
    REQUIRE(r.error().message == "v");
}

#pragma once

#include <exception>
#include <string>
#include <vector>

namespace fsrs {

struct ValidationError : std::exception {
    std::string message;
    std::vector<std::string> details;

    ValidationError() = default;
    ValidationError(std::string msg, std::vector<std::string> det = {})
        : message(std::move(msg)), details(std::move(det)) {}

    const char* what() const noexcept override { return message.c_str(); }
};

struct ParseError : std::exception {
    std::string message;

    ParseError() = default;
    explicit ParseError(std::string msg) : message(std::move(msg)) {}

    const char* what() const noexcept override { return message.c_str(); }
};

}  // namespace fsrs

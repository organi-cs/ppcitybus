#include "core/error.hpp"

namespace ppbus {

namespace {

std::string format_error(const std::string& source, std::size_t line, const std::string& message) {
    if (line == 0) return source + ": " + message;
    return source + ":" + std::to_string(line) + ": " + message;
}

}  // namespace

DataError::DataError(const std::string& source, std::size_t line, const std::string& message)
    : std::runtime_error(format_error(source, line, message)) {}

}  // namespace ppbus

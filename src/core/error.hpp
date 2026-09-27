#pragma once

#include <cstddef>
#include <stdexcept>
#include <string>

namespace ppbus {

// what() is "source:line: message" (or "source: message" when line is 0)
// so data errors point straight at the row to fix.
class DataError : public std::runtime_error {
public:
    DataError(const std::string& source, std::size_t line, const std::string& message);
};

}  // namespace ppbus

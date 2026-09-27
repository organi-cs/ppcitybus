#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/config.hpp"
#include "core/network.hpp"

namespace ppbus {

enum class Severity { Warning, Note };

struct Finding {
    Severity severity = Severity::Warning;
    std::string source;    // file name, or "network" for whole-network findings
    std::size_t line = 0;  // 0 when the finding has no single line
    std::string message;
};

// Likely data-entry mistakes in a network that loaded without errors.
std::vector<Finding> validate_network(const Network& net, const Config& config);

// "stops.csv:12: warning: message", compiler style so editors can jump to it.
std::string format_finding(const Finding& finding);

}  // namespace ppbus

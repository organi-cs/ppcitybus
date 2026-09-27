#pragma once

#include <filesystem>
#include <string>
#include <string_view>

#include "core/error.hpp"

namespace ppbus {

struct Config {
    int fare_khr = 1500;                     // per boarding; transfers are not free
    double walk_speed_kmh = 4.5;
    double walk_radius_m = 400;              // stop-to-stop walking edges
    double access_radius_m = 800;            // lat,lon origin or destination to stops
    double detour_factor = 1.3;              // street distance / straight-line distance
    int transfer_penalty_s = 120;            // added to every boarding after the first
    double walk_weight = 4.0;                // walking-time multiplier in "walk" mode
    double default_headway_min = 15;         // for lines.csv rows without headway_min
    double default_min_between_stops = 2.0;  // for rows without avg_min_between_stops
};

// key=value lines; '#' starts a comment line. Unknown keys are errors so typos
// don't silently fall back to defaults. Throws DataError.
Config parse_config(std::string_view text, const std::string& source);

Config load_config_file(const std::filesystem::path& path);

}  // namespace ppbus

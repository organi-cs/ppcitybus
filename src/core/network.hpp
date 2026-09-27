#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/config.hpp"
#include "core/csv.hpp"
#include "core/geo.hpp"

namespace ppbus {

using StopIndex = std::uint32_t;
using StationIndex = std::uint32_t;

struct Stop {
    std::string id;
    std::string name_en;
    std::string name_km;
    LatLon pos;
    std::vector<std::string> aliases;
    std::string osm_id;
    StationIndex station = 0;
    std::size_t source_line = 0;
};

// Platforms that riders think of as one place, e.g. both sides of a street.
// Keyed by the stops.csv `station` column, or the lowercased English name.
struct Station {
    std::string key;
    std::vector<StopIndex> stops;
};

// One direction of one line: the unit a bus actually runs.
struct Pattern {
    std::string line;
    std::string direction;
    std::string name;
    int headway_s = 0;
    int ride_s = 0;  // between consecutive stops
    std::string color;
    bool approx = false;  // not yet checked against the official source
    std::vector<StopIndex> stops;
    std::size_t source_line = 0;

    bool is_loop() const { return stops.size() > 2 && stops.front() == stops.back(); }
};

struct Network {
    std::string stops_source;  // file names, for messages
    std::string lines_source;
    std::vector<Stop> stops;
    std::vector<Station> stations;
    std::vector<Pattern> patterns;
    std::unordered_map<std::string, StopIndex> stop_by_id;

    std::optional<StopIndex> find_stop(std::string_view id) const;
};

// Every content problem found in one pass, so a data file can be fixed in one go.
class NetworkError : public std::runtime_error {
public:
    explicit NetworkError(std::vector<std::string> messages);
    const std::vector<std::string>& messages() const { return messages_; }

private:
    std::vector<std::string> messages_;
};

// Throws NetworkError listing every problem in the tables.
Network build_network(const CsvTable& stops, const CsvTable& lines, const Config& config);

// Reads dir/stops.csv and dir/lines.csv. Throws DataError for unreadable or
// malformed CSV and NetworkError for bad content.
Network load_network(const std::filesystem::path& dir, const Config& config);

}  // namespace ppbus

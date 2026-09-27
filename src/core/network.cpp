#include "core/network.hpp"

#include <charconv>
#include <cmath>
#include <map>
#include <utility>

namespace ppbus {

namespace {

std::string join_lines(const std::vector<std::string>& messages) {
    std::string out;
    for (const auto& m : messages) {
        if (!out.empty()) out += '\n';
        out += m;
    }
    return out;
}

std::string where(const CsvTable& table, std::size_t line) {
    return table.source + ":" + std::to_string(line) + ": ";
}

std::string ascii_lower(std::string_view s) {
    std::string out(s);
    for (char& c : out) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return out;
}

std::string_view trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t");
    if (first == std::string_view::npos) return {};
    const auto last = s.find_last_not_of(" \t");
    return s.substr(first, last - first + 1);
}

std::vector<std::string> split_list(std::string_view s) {
    std::vector<std::string> out;
    while (true) {
        const auto semi = s.find(';');
        const auto item = trim(s.substr(0, semi));
        if (!item.empty()) out.emplace_back(item);
        if (semi == std::string_view::npos) return out;
        s.remove_prefix(semi + 1);
    }
}

std::optional<double> parse_double(std::string_view s) {
    double value = 0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (s.empty() || ec != std::errc{} || end != s.data() + s.size() || !std::isfinite(value)) return std::nullopt;
    return value;
}

bool is_hex_color(std::string_view s) {
    if (s.size() != 7 || s[0] != '#') return false;
    for (char c : s.substr(1)) {
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
        if (!hex) return false;
    }
    return true;
}

// Column positions, with missing optional columns reading as empty strings.
class Columns {
public:
    Columns(const CsvTable& table, std::vector<std::string>& errors) : table_(table), errors_(errors) {}

    std::optional<std::size_t> require(std::string_view name) {
        auto col = table_.column(name);
        if (!col) errors_.push_back(where(table_, 1) + "missing column '" + std::string(name) + "'");
        return col;
    }

    std::optional<std::size_t> optional(std::string_view name) const { return table_.column(name); }

    static std::string_view get(const CsvRow& row, std::optional<std::size_t> col) {
        return col ? std::string_view(row.fields[*col]) : std::string_view{};
    }

private:
    const CsvTable& table_;
    std::vector<std::string>& errors_;
};

void load_stops(const CsvTable& table, Network& net, std::vector<std::string>& errors) {
    Columns cols(table, errors);
    const auto id = cols.require("id");
    const auto name_en = cols.require("name_en");
    const auto lat = cols.require("lat");
    const auto lon = cols.require("lon");
    if (!id || !name_en || !lat || !lon) return;
    const auto name_km = cols.optional("name_km");
    const auto station = cols.optional("station");
    const auto aliases = cols.optional("aliases");
    const auto osm_id = cols.optional("osm_id");

    std::map<std::string, StationIndex> station_by_key;
    for (const auto& row : table.rows) {
        const std::string at = where(table, row.line);
        Stop stop;
        stop.id = Columns::get(row, id);
        stop.name_en = Columns::get(row, name_en);
        stop.name_km = Columns::get(row, name_km);
        stop.aliases = split_list(Columns::get(row, aliases));
        stop.osm_id = Columns::get(row, osm_id);
        stop.source_line = row.line;

        if (stop.id.empty()) {
            errors.push_back(at + "empty id");
            continue;
        }
        if (const auto prev = net.find_stop(stop.id)) {
            errors.push_back(at + "duplicate stop id '" + stop.id + "' (first on line " +
                             std::to_string(net.stops[*prev].source_line) + ")");
            continue;
        }
        if (stop.name_en.empty()) errors.push_back(at + "empty name_en");

        const auto lat_text = Columns::get(row, lat);
        const auto lat_value = parse_double(lat_text);
        if (lat_value && *lat_value >= -90 && *lat_value <= 90) {
            stop.pos.lat = *lat_value;
        } else {
            errors.push_back(at + "invalid lat '" + std::string(lat_text) + "'");
        }
        const auto lon_text = Columns::get(row, lon);
        const auto lon_value = parse_double(lon_text);
        if (lon_value && *lon_value >= -180 && *lon_value <= 180) {
            stop.pos.lon = *lon_value;
        } else {
            errors.push_back(at + "invalid lon '" + std::string(lon_text) + "'");
        }

        const auto explicit_station = Columns::get(row, station);
        const std::string key = ascii_lower(explicit_station.empty() ? stop.name_en : explicit_station);
        const auto [it, inserted] = station_by_key.try_emplace(key, static_cast<StationIndex>(net.stations.size()));
        if (inserted) net.stations.push_back({key, {}});
        stop.station = it->second;

        const auto index = static_cast<StopIndex>(net.stops.size());
        net.stations[stop.station].stops.push_back(index);
        net.stop_by_id.emplace(stop.id, index);
        net.stops.push_back(std::move(stop));
    }
}

void load_lines(const CsvTable& table, const Config& config, Network& net, std::vector<std::string>& errors) {
    Columns cols(table, errors);
    const auto line = cols.require("line");
    const auto direction = cols.require("direction");
    const auto stops = cols.require("stops");
    if (!line || !direction || !stops) return;
    const auto name = cols.optional("name");
    const auto headway = cols.optional("headway_min");
    const auto ride = cols.optional("avg_min_between_stops");
    const auto color = cols.optional("color");
    const auto approx = cols.optional("approx");

    std::map<std::pair<std::string, std::string>, std::size_t> first_line_of;
    for (const auto& row : table.rows) {
        const std::string at = where(table, row.line);
        const std::size_t errors_before = errors.size();
        Pattern p;
        p.line = Columns::get(row, line);
        p.direction = Columns::get(row, direction);
        p.name = Columns::get(row, name);
        p.color = Columns::get(row, color);
        p.source_line = row.line;

        if (p.line.empty()) errors.push_back(at + "missing line number");
        if (p.direction.empty()) errors.push_back(at + "missing direction");
        if (!p.line.empty() && !p.direction.empty()) {
            const auto [it, inserted] = first_line_of.try_emplace({p.line, p.direction}, row.line);
            if (!inserted) {
                errors.push_back(at + "line " + p.line + " direction " + p.direction +
                                 " is defined twice (first on line " + std::to_string(it->second) + ")");
            }
        }

        const auto minutes = [&](std::optional<std::size_t> col, std::string_view col_name, double fallback) {
            const auto text = Columns::get(row, col);
            if (text.empty()) return static_cast<int>(std::lround(fallback * 60));
            const auto value = parse_double(text);
            if (!value || *value <= 0) {
                errors.push_back(at + "invalid " + std::string(col_name) + " '" + std::string(text) + "'");
                return 0;
            }
            return static_cast<int>(std::lround(*value * 60));
        };
        p.headway_s = minutes(headway, "headway_min", config.default_headway_min);
        p.ride_s = minutes(ride, "avg_min_between_stops", config.default_min_between_stops);

        if (!p.color.empty() && !is_hex_color(p.color)) {
            errors.push_back(at + "invalid color '" + p.color + "' (expected #RRGGBB)");
        }
        const auto approx_text = Columns::get(row, approx);
        if (approx_text == "1") {
            p.approx = true;
        } else if (!approx_text.empty() && approx_text != "0") {
            errors.push_back(at + "invalid approx '" + std::string(approx_text) + "' (expected 0 or 1)");
        }

        const auto ids = split_list(Columns::get(row, stops));
        for (std::size_t i = 0; i < ids.size(); ++i) {
            if (const auto stop = net.find_stop(ids[i])) {
                p.stops.push_back(*stop);
            } else {
                errors.push_back(at + "unknown stop '" + ids[i] + "' at position " + std::to_string(i + 1));
            }
        }
        if (ids.size() < 2) {
            errors.push_back(at + "line " + p.line + " direction " + p.direction + " needs at least 2 stops");
        }

        if (errors.size() == errors_before) net.patterns.push_back(std::move(p));
    }
}

}  // namespace

std::optional<StopIndex> Network::find_stop(std::string_view id) const {
    const auto it = stop_by_id.find(std::string(id));
    if (it == stop_by_id.end()) return std::nullopt;
    return it->second;
}

NetworkError::NetworkError(std::vector<std::string> messages)
    : std::runtime_error(join_lines(messages)), messages_(std::move(messages)) {}

Network build_network(const CsvTable& stops, const CsvTable& lines, const Config& config) {
    Network net;
    std::vector<std::string> errors;
    load_stops(stops, net, errors);
    load_lines(lines, config, net, errors);
    if (!errors.empty()) throw NetworkError(std::move(errors));
    return net;
}

Network load_network(const std::filesystem::path& dir, const Config& config) {
    const CsvTable stops = read_csv_file(dir / "stops.csv");
    const CsvTable lines = read_csv_file(dir / "lines.csv");
    return build_network(stops, lines, config);
}

}  // namespace ppbus

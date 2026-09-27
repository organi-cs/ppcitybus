#include "core/validate.hpp"

#include <algorithm>
#include <cstdio>
#include <map>
#include <numeric>

namespace ppbus {

namespace {

// Generous box around Phnom Penh, Takhmao and the SEZ; anything outside is
// almost always swapped lat/lon or a typo.
constexpr double kMinLat = 11.3, kMaxLat = 11.8, kMinLon = 104.7, kMaxLon = 105.1;
constexpr double kFarBetweenStopsM = 3000;
constexpr std::size_t kIdsPerPart = 5;

std::string fixed(double value, int decimals) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "%.*f", decimals, value);
    return buf;
}

class DisjointSets {
public:
    explicit DisjointSets(std::size_t n) : parent_(n) { std::iota(parent_.begin(), parent_.end(), 0); }

    std::size_t find(std::size_t x) {
        while (parent_[x] != x) x = parent_[x] = parent_[parent_[x]];
        return x;
    }

    void unite(std::size_t a, std::size_t b) {
        a = find(a), b = find(b);
        if (a != b) parent_[std::max(a, b)] = std::min(a, b);
    }

private:
    std::vector<std::size_t> parent_;
};

// Groups of served stops that neither buses nor walking connect.
std::vector<std::vector<StopIndex>> disconnected_parts(const Network& net, const Config& config,
                                                       const std::vector<bool>& served) {
    DisjointSets sets(net.stops.size());
    for (const auto& p : net.patterns) {
        for (std::size_t i = 1; i < p.stops.size(); ++i) sets.unite(p.stops[i - 1], p.stops[i]);
    }
    for (std::size_t a = 0; a < net.stops.size(); ++a) {
        if (!served[a]) continue;
        for (std::size_t b = a + 1; b < net.stops.size(); ++b) {
            if (served[b] && haversine_m(net.stops[a].pos, net.stops[b].pos) * config.detour_factor <=
                                 config.walk_radius_m) {
                sets.unite(a, b);
            }
        }
    }

    std::map<std::size_t, std::vector<StopIndex>> by_root;  // roots are the smallest member index
    for (std::size_t s = 0; s < net.stops.size(); ++s) {
        if (served[s]) by_root[sets.find(s)].push_back(static_cast<StopIndex>(s));
    }
    std::vector<std::vector<StopIndex>> parts;
    for (auto& [root, members] : by_root) parts.push_back(std::move(members));
    std::stable_sort(parts.begin(), parts.end(), [](const auto& a, const auto& b) { return a.size() > b.size(); });
    return parts;
}

}  // namespace

std::vector<Finding> validate_network(const Network& net, const Config& config) {
    std::vector<Finding> out;
    const auto warn = [&](const std::string& source, std::size_t line, std::string message) {
        out.push_back({Severity::Warning, source, line, std::move(message)});
    };
    const auto note = [&](const std::string& source, std::size_t line, std::string message) {
        out.push_back({Severity::Note, source, line, std::move(message)});
    };

    std::vector<bool> served(net.stops.size(), false);
    std::map<std::string, int> directions_per_line;
    for (const auto& p : net.patterns) {
        for (StopIndex s : p.stops) served[s] = true;
        ++directions_per_line[p.line];
    }

    std::size_t missing_km = 0;
    for (std::size_t i = 0; i < net.stops.size(); ++i) {
        const Stop& s = net.stops[i];
        const std::string label = "stop " + s.id + " '" + s.name_en + "'";
        if (s.pos.lat < kMinLat || s.pos.lat > kMaxLat || s.pos.lon < kMinLon || s.pos.lon > kMaxLon) {
            warn(net.stops_source, s.source_line,
                 label + " is outside the Phnom Penh area (" + fixed(s.pos.lat, 5) + ", " + fixed(s.pos.lon, 5) + ")");
        }
        if (!served[i]) warn(net.stops_source, s.source_line, label + " is not served by any line");
        if (s.name_km.empty()) ++missing_km;
    }

    for (const auto& p : net.patterns) {
        const std::string label = "line " + p.line + " direction " + p.direction;
        for (std::size_t i = 1; i < p.stops.size(); ++i) {
            const Stop& a = net.stops[p.stops[i - 1]];
            const Stop& b = net.stops[p.stops[i]];
            const std::string positions = "positions " + std::to_string(i) + " and " + std::to_string(i + 1);
            if (p.stops[i - 1] == p.stops[i]) {
                warn(net.lines_source, p.source_line, label + " visits " + a.id + " twice in a row (" + positions + ")");
                continue;
            }
            const double d = haversine_m(a.pos, b.pos);
            if (d > kFarBetweenStopsM) {
                warn(net.lines_source, p.source_line,
                     label + ": " + a.id + " -> " + b.id + " is " + fixed(d / 1000, 1) + " km (" + positions +
                         "); check the stop order or coordinates");
            }
        }
        if (p.approx) note(net.lines_source, p.source_line, label + " is marked approx");
        if (directions_per_line[p.line] == 1) {
            note(net.lines_source, p.source_line, "line " + p.line + " has only one direction");
        }
    }

    const auto parts = disconnected_parts(net, config, served);
    if (parts.size() > 1) {
        std::string cut_off;
        for (std::size_t k = 1; k < parts.size(); ++k) {
            if (k > 1) cut_off += " | ";
            for (std::size_t i = 0; i < std::min(parts[k].size(), kIdsPerPart); ++i) {
                cut_off += (i ? ", " : "") + net.stops[parts[k][i]].id;
            }
            if (parts[k].size() > kIdsPerPart) cut_off += " (+" + std::to_string(parts[k].size() - kIdsPerPart) + " more)";
        }
        warn("network", 0,
             "network splits into " + std::to_string(parts.size()) +
                 " disconnected parts; cut off from the largest: " + cut_off);
    }

    if (missing_km > 0) {
        note(net.stops_source, 0,
             std::to_string(missing_km) + " of " + std::to_string(net.stops.size()) + " stops have no name_km");
    }
    return out;
}

std::string format_finding(const Finding& finding) {
    std::string out = finding.source;
    if (finding.line > 0) out += ":" + std::to_string(finding.line);
    out += finding.severity == Severity::Warning ? ": warning: " : ": note: ";
    return out + finding.message;
}

}  // namespace ppbus

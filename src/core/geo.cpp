#include "core/geo.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>

namespace ppbus {

namespace {

constexpr double kPi = 3.14159265358979323846;

double radians(double deg) { return deg * kPi / 180.0; }

std::string_view trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t");
    if (first == std::string_view::npos) return {};
    const auto last = s.find_last_not_of(" \t");
    return s.substr(first, last - first + 1);
}

std::optional<double> parse_finite(std::string_view s) {
    s = trim(s);
    if (s.empty()) return std::nullopt;
    double value = 0.0;
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || end != s.data() + s.size() || !std::isfinite(value)) return std::nullopt;
    return value;
}

}  // namespace

double haversine_m(LatLon a, LatLon b) {
    const double dlat = radians(b.lat - a.lat);
    const double dlon = radians(b.lon - a.lon);
    const double h = std::sin(dlat / 2) * std::sin(dlat / 2) +
                     std::cos(radians(a.lat)) * std::cos(radians(b.lat)) * std::sin(dlon / 2) * std::sin(dlon / 2);
    return 2 * kEarthRadiusM * std::asin(std::sqrt(std::min(1.0, h)));
}

std::optional<LatLon> parse_latlon(std::string_view text) {
    const auto comma = text.find(',');
    if (comma == std::string_view::npos) return std::nullopt;
    const auto lat = parse_finite(text.substr(0, comma));
    const auto lon = parse_finite(text.substr(comma + 1));
    if (!lat || !lon) return std::nullopt;
    if (*lat < -90.0 || *lat > 90.0 || *lon < -180.0 || *lon > 180.0) return std::nullopt;
    return LatLon{*lat, *lon};
}

}  // namespace ppbus

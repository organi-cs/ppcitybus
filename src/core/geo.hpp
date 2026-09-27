#pragma once

#include <optional>
#include <string_view>

namespace ppbus {

struct LatLon {
    double lat = 0.0;
    double lon = 0.0;
};

constexpr double kEarthRadiusM = 6371008.8;

// Great-circle distance in metres.
double haversine_m(LatLon a, LatLon b);

// Parses "lat,lon" in decimal degrees; surrounding spaces are allowed.
std::optional<LatLon> parse_latlon(std::string_view text);

}  // namespace ppbus

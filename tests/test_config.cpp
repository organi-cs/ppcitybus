#include <doctest.h>

#include "core/config.hpp"

using ppbus::Config;
using ppbus::DataError;
using ppbus::parse_config;

namespace {

std::string error_of(std::string_view text) {
    try {
        parse_config(text, "network.ini");
    } catch (const DataError& e) {
        return e.what();
    }
    return "no error";
}

}  // namespace

TEST_CASE("empty config gives the documented defaults") {
    const Config c = parse_config("", "network.ini");
    CHECK(c.fare_khr == 1500);
    CHECK(c.walk_speed_kmh == 4.5);
    CHECK(c.walk_radius_m == 400);
    CHECK(c.access_radius_m == 800);
    CHECK(c.detour_factor == 1.3);
    CHECK(c.transfer_penalty_s == 120);
    CHECK(c.walk_weight == 4.0);
    CHECK(c.default_headway_min == 15);
    CHECK(c.default_min_between_stops == 2.0);
}

TEST_CASE("values override defaults; comments, blanks, spaces and CRLF are fine") {
    const Config c = parse_config(
        "\xef\xbb\xbf# Phnom Penh defaults\r\n"
        "fare_khr = 2000\r\n"
        "\r\n"
        "  walk_radius_m=250  \r\n"
        "detour_factor=1.0\r\n",
        "network.ini");
    CHECK(c.fare_khr == 2000);
    CHECK(c.walk_radius_m == 250);
    CHECK(c.detour_factor == 1.0);
    CHECK(c.walk_speed_kmh == 4.5);
}

TEST_CASE("config errors name the line") {
    CHECK(error_of("fare_khr\n") == "network.ini:1: expected key=value");
    CHECK(error_of("\nwalk_radius=400\n") == "network.ini:2: unknown setting 'walk_radius'");
    CHECK(error_of("fare_khr=cheap\n") == "network.ini:1: invalid value for 'fare_khr': 'cheap'");
    CHECK(error_of("fare_khr=1500.5\n") == "network.ini:1: invalid value for 'fare_khr': '1500.5'");
    CHECK(error_of("fare_khr=1\nfare_khr=2\n") == "network.ini:2: 'fare_khr' is set twice");
    CHECK(error_of("walk_speed_kmh=0\n") == "network.ini:1: 'walk_speed_kmh' must be > 0");
    CHECK(error_of("detour_factor=0.9\n") == "network.ini:1: 'detour_factor' must be >= 1");
    CHECK(error_of("walk_radius_m=-1\n") == "network.ini:1: 'walk_radius_m' must be >= 0");
}

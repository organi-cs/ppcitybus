#include <doctest.h>

#include <filesystem>
#include <fstream>

#include "core/network.hpp"

using ppbus::build_network;
using ppbus::Config;
using ppbus::load_network;
using ppbus::Network;
using ppbus::NetworkError;
using ppbus::parse_csv;

namespace {

const char* const kStops =
    "id,name_en,name_km,lat,lon,station,aliases,osm_id\n"
    "S1,Central Market,\xe1\x9e\x95\xe1\x9f\x92\xe1\x9e\x9f\xe1\x9e\xb6\xe1\x9e\x9a,11.5696,104.9210,,Phsar Thmei; Phsar Thmey ,3147349034\n"
    "S2,central market,,11.5699,104.9214,,,\n"
    "S3,Old Stadium,,11.5640,104.9110,old-stadium,,\n"
    "S4,Old Stadium Roundabout,,11.5645,104.9105,old-stadium,,\n"
    "S5,Aeon Mall II,,11.5981,104.8834,,,7244107487\n";

const char* const kLines =
    "line,direction,name,headway_min,avg_min_between_stops,color,approx,stops\n"
    "3,0,Central Market to Aeon,10,2.5,#1f77b4,0,S1;S3;S5;\n"
    "3,1,,,,,1,S5;S4;S2\n"
    "12,loop,,,,,,S1;S3;S5;S1\n";

Network build(const std::string& stops, const std::string& lines, const Config& config = {}) {
    return build_network(parse_csv(stops, "stops.csv"), parse_csv(lines, "lines.csv"), config);
}

std::vector<std::string> errors_of(const std::string& stops, const std::string& lines) {
    try {
        build(stops, lines);
    } catch (const NetworkError& e) {
        return e.messages();
    }
    return {};
}

}  // namespace

TEST_CASE("stops load with names, aliases and ids") {
    const Network n = build(kStops, kLines);
    REQUIRE(n.stops.size() == 5);
    const auto& s1 = n.stops[0];
    CHECK(s1.id == "S1");
    CHECK(s1.name_en == "Central Market");
    CHECK(s1.name_km == "\xe1\x9e\x95\xe1\x9f\x92\xe1\x9e\x9f\xe1\x9e\xb6\xe1\x9e\x9a");
    CHECK(s1.pos.lat == doctest::Approx(11.5696));
    CHECK(s1.aliases == std::vector<std::string>{"Phsar Thmei", "Phsar Thmey"});
    CHECK(s1.osm_id == "3147349034");
    CHECK(s1.source_line == 2);
    CHECK(n.find_stop("S5") == 4u);
    CHECK_FALSE(n.find_stop("S9"));
}

TEST_CASE("stations group by explicit key, else by case-insensitive name") {
    const Network n = build(kStops, kLines);
    REQUIRE(n.stations.size() == 3);
    CHECK(n.stops[0].station == n.stops[1].station);
    CHECK(n.stops[2].station == n.stops[3].station);
    CHECK(n.stops[0].station != n.stops[2].station);
    CHECK(n.stations[n.stops[0].station].key == "central market");
    CHECK(n.stations[n.stops[2].station].key == "old-stadium");
    CHECK(n.stations[n.stops[2].station].stops == std::vector<ppbus::StopIndex>{2, 3});
}

TEST_CASE("patterns take explicit values or config defaults") {
    Config config;
    config.default_headway_min = 20;
    config.default_min_between_stops = 1.5;
    const Network n = build(kStops, kLines, config);
    REQUIRE(n.patterns.size() == 3);

    const auto& p0 = n.patterns[0];
    CHECK(p0.line == "3");
    CHECK(p0.direction == "0");
    CHECK(p0.name == "Central Market to Aeon");
    CHECK(p0.headway_s == 600);
    CHECK(p0.ride_s == 150);
    CHECK(p0.color == "#1f77b4");
    CHECK_FALSE(p0.approx);
    CHECK(p0.stops == std::vector<ppbus::StopIndex>{0, 2, 4});  // trailing ';' ignored
    CHECK_FALSE(p0.is_loop());

    const auto& p1 = n.patterns[1];
    CHECK(p1.headway_s == 1200);
    CHECK(p1.ride_s == 90);
    CHECK(p1.approx);
    CHECK(p1.source_line == 3);

    CHECK(n.patterns[2].is_loop());
}

TEST_CASE("every content error is reported at once with its line") {
    const std::string stops =
        "id,name_en,lat,lon\n"
        "S1,A,11.5,104.9\n"
        "S1,B,11.5,104.9\n"
        ",C,11.5,104.9\n"
        "S4,,11.5,104.9\n"
        "S5,E,north,104.9\n"
        "S6,F,11.5,200\n";
    const std::string lines =
        "line,direction,headway_min,avg_min_between_stops,color,approx,stops\n"
        "1,0,,,,,S1;S9\n"
        "1,0,,,,,S1;S1\n"
        ",0,,,,,S1;S5\n"
        "2,0,0,,,,S1;S5\n"
        "2,1,,fast,,,S1;S5\n"
        "3,0,,,blue,,S1;S5\n"
        "3,1,,,,yes,S1;S5\n"
        "4,0,,,,,S1\n";
    CHECK(errors_of(stops, lines) == std::vector<std::string>{
        "stops.csv:3: duplicate stop id 'S1' (first on line 2)",
        "stops.csv:4: empty id",
        "stops.csv:5: empty name_en",
        "stops.csv:6: invalid lat 'north'",
        "stops.csv:7: invalid lon '200'",
        "lines.csv:2: unknown stop 'S9' at position 2",
        "lines.csv:3: line 1 direction 0 is defined twice (first on line 2)",
        "lines.csv:4: missing line number",
        "lines.csv:5: invalid headway_min '0'",
        "lines.csv:6: invalid avg_min_between_stops 'fast'",
        "lines.csv:7: invalid color 'blue' (expected #RRGGBB)",
        "lines.csv:8: invalid approx 'yes' (expected 0 or 1)",
        "lines.csv:9: line 4 direction 0 needs at least 2 stops",
    });
}

TEST_CASE("missing required columns stop loading early") {
    CHECK(errors_of("id,name_en,lat\n", "line,direction,stops\n") ==
          std::vector<std::string>{"stops.csv:1: missing column 'lon'"});
    CHECK(errors_of("id,name_en,lat,lon\n", "line,stops\n") ==
          std::vector<std::string>{"lines.csv:1: missing column 'direction'"});
}

TEST_CASE("load_network reads stops.csv and lines.csv from a directory") {
    const auto dir = std::filesystem::temp_directory_path() / "ppbus_test_network";
    std::filesystem::create_directories(dir);
    std::ofstream(dir / "stops.csv", std::ios::binary) << kStops;
    std::ofstream(dir / "lines.csv", std::ios::binary) << kLines;

    const Network n = load_network(dir, Config{});
    CHECK(n.stops.size() == 5);
    CHECK(n.patterns.size() == 3);

    std::filesystem::remove(dir / "lines.csv");
    CHECK_THROWS_AS(load_network(dir, Config{}), ppbus::DataError);
    std::filesystem::remove_all(dir);
}

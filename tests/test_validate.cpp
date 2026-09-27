#include <doctest.h>

#include <tuple>

#include "core/validate.hpp"

using ppbus::build_network;
using ppbus::Config;
using ppbus::Finding;
using ppbus::format_finding;
using ppbus::parse_csv;
using ppbus::validate_network;

namespace {

std::vector<std::string> findings_for(const std::string& stops, const std::string& lines) {
    const Config config;
    const auto net = build_network(parse_csv(stops, "stops.csv"), parse_csv(lines, "lines.csv"), config);
    std::vector<std::string> out;
    for (const Finding& f : validate_network(net, config)) out.push_back(format_finding(f));
    return out;
}

}  // namespace

TEST_CASE("a clean network has no findings") {
    CHECK(findings_for(
              "id,name_en,name_km,lat,lon\n"
              "S1,A,a,11.5600,104.9200\n"
              "S2,B,b,11.5610,104.9200\n",
              "line,direction,stops\n"
              "1,0,S1;S2\n"
              "1,1,S2;S1\n")
              .empty());
}

TEST_CASE("each likely mistake is reported once, stops first, then lines, then network") {
    // S2-S3 is 0.039 deg of latitude = 4.3 km. S4 is 222 m from S2 (walkable, unserved).
    // S6-S7 are about 1.1 km east of S1, too far to walk, so line 2 is cut off.
    const std::string stops =
        "id,name_en,name_km,lat,lon\n"
        "S1,A,a,11.5600,104.9200\n"
        "S2,B,,11.5610,104.9200\n"
        "S3,C,,11.6000,104.9200\n"
        "S4,D,,11.5630,104.9200\n"
        "S5,Far,,12.5000,104.9000\n"
        "S6,E,,11.5600,104.9300\n"
        "S7,F,,11.5600,104.9310\n";
    const std::string lines =
        "line,direction,approx,stops\n"
        "1,0,0,S1;S2;S2;S3\n"
        "1,1,1,S3;S2;S1\n"
        "2,0,,S6;S7\n";
    CHECK(findings_for(stops, lines) == std::vector<std::string>{
        "stops.csv:5: warning: stop S4 'D' is not served by any line",
        "stops.csv:6: warning: stop S5 'Far' is outside the Phnom Penh area (12.50000, 104.90000)",
        "stops.csv:6: warning: stop S5 'Far' is not served by any line",
        "lines.csv:2: warning: line 1 direction 0 visits S2 twice in a row (positions 2 and 3)",
        "lines.csv:2: warning: line 1 direction 0: S2 -> S3 is 4.3 km (positions 3 and 4); "
        "check the stop order or coordinates",
        "lines.csv:3: warning: line 1 direction 1: S3 -> S2 is 4.3 km (positions 1 and 2); "
        "check the stop order or coordinates",
        "lines.csv:3: note: line 1 direction 1 is marked approx",
        "lines.csv:4: note: line 2 has only one direction",
        "network: warning: network splits into 2 disconnected parts; cut off from the largest: S6, S7",
        "stops.csv: note: 6 of 7 stops have no name_km",
    });
}

TEST_CASE("loop lines are not flagged for returning to their first stop") {
    CHECK(findings_for(
              "id,name_en,name_km,lat,lon\n"
              "S1,A,a,11.5600,104.9200\n"
              "S2,B,b,11.5610,104.9200\n"
              "S3,C,c,11.5610,104.9210\n",
              "line,direction,stops\n"
              "12,loop,S1;S2;S3;S1\n")
              == std::vector<std::string>{"lines.csv:2: note: line 12 has only one direction"});
}

TEST_CASE("long disconnected parts are abbreviated") {
    // Main part: 9 stops on line 1. Cut-off part: 7 stops on line 2, 15 km away.
    std::string stops = "id,name_en,name_km,lat,lon\n";
    std::string lines = "line,direction,stops\n";
    for (const auto& [prefix, count, lat] : {std::tuple{"M", 9, "11.50"}, std::tuple{"X", 7, "11.60"}}) {
        std::string forward;
        std::string backward;
        for (int i = 1; i <= count; ++i) {
            const std::string id = prefix + std::to_string(i);
            stops += id + ",n,k," + lat + ",104.90\n";
            forward += (i > 1 ? ";" : "") + id;
            backward = id + (i > 1 ? ";" : "") + backward;
        }
        const std::string line = prefix == std::string("M") ? "1" : "2";
        lines += line + ",0," + forward + "\n" + line + ",1," + backward + "\n";
    }
    const auto found = findings_for(stops, lines);
    REQUIRE(found.size() == 1);
    CHECK(found[0] ==
          "network: warning: network splits into 2 disconnected parts; cut off from the largest: "
          "X1, X2, X3, X4, X5 (+2 more)");
}

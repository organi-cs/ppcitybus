#include <doctest.h>

#include "core/geo.hpp"

using ppbus::haversine_m;
using ppbus::LatLon;
using ppbus::parse_latlon;

namespace {
// pi * R / 180 with the mean Earth radius 6371008.8 m.
constexpr double kOneDegreeM = 111195.08;
}  // namespace

TEST_CASE("haversine matches known distances") {
    CHECK(haversine_m({11.0, 104.9}, {12.0, 104.9}) == doctest::Approx(kOneDegreeM).epsilon(0.001));
    CHECK(haversine_m({0.0, 0.0}, {0.0, 1.0}) == doctest::Approx(kOneDegreeM).epsilon(0.001));
    CHECK(haversine_m({0.0, 0.0}, {0.0, 180.0}) == doctest::Approx(180 * kOneDegreeM).epsilon(0.001));
}

TEST_CASE("haversine is zero for the same point and symmetric") {
    const LatLon central_market{11.5696, 104.9210};
    const LatLon aeon_mall_2{11.6002, 104.8854};
    CHECK(haversine_m(central_market, central_market) == 0.0);
    CHECK(haversine_m(central_market, aeon_mall_2) == haversine_m(aeon_mall_2, central_market));
}

TEST_CASE("longitude degrees shrink with latitude") {
    // At 60 degrees a degree of longitude is about half as long as at the equator.
    CHECK(haversine_m({60.0, 0.0}, {60.0, 1.0}) == doctest::Approx(kOneDegreeM / 2).epsilon(0.002));
}

TEST_CASE("parse_latlon accepts lat,lon with optional spaces") {
    auto p = parse_latlon("11.56,104.92");
    REQUIRE(p);
    CHECK(p->lat == doctest::Approx(11.56));
    CHECK(p->lon == doctest::Approx(104.92));

    p = parse_latlon("  -11.5 ,  -104 ");
    REQUIRE(p);
    CHECK(p->lat == doctest::Approx(-11.5));
    CHECK(p->lon == doctest::Approx(-104.0));
}

TEST_CASE("parse_latlon rejects malformed or out-of-range input") {
    CHECK_FALSE(parse_latlon(""));
    CHECK_FALSE(parse_latlon("11.56"));
    CHECK_FALSE(parse_latlon("11.56,"));
    CHECK_FALSE(parse_latlon(",104.92"));
    CHECK_FALSE(parse_latlon("a,b"));
    CHECK_FALSE(parse_latlon("11.56,104.92,3"));
    CHECK_FALSE(parse_latlon("11.56,104.92x"));
    CHECK_FALSE(parse_latlon("91,0"));
    CHECK_FALSE(parse_latlon("0,181"));
    CHECK_FALSE(parse_latlon("nan,0"));
    CHECK_FALSE(parse_latlon("0,inf"));
    CHECK_FALSE(parse_latlon("Central Market"));
}

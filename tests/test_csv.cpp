#include <doctest.h>

#include <filesystem>
#include <fstream>

#include "core/csv.hpp"

using ppbus::DataError;
using ppbus::parse_csv;
using ppbus::read_csv_file;

namespace {

std::string error_of(std::string_view text) {
    try {
        parse_csv(text, "t.csv");
    } catch (const DataError& e) {
        return e.what();
    }
    return "no error";
}

}  // namespace

TEST_CASE("header and rows with source line numbers") {
    const auto t = parse_csv(
        "id,name\n"
        "# comment line\n"
        "S1,Central Market\n"
        "\n"
        "S2,Old Stadium\n",
        "stops.csv");
    CHECK(t.source == "stops.csv");
    CHECK(t.header == std::vector<std::string>{"id", "name"});
    REQUIRE(t.rows.size() == 2);
    CHECK(t.rows[0].line == 3);
    CHECK(t.rows[0].fields == std::vector<std::string>{"S1", "Central Market"});
    CHECK(t.rows[1].line == 5);
    CHECK(t.rows[1].fields[1] == "Old Stadium");
}

TEST_CASE("quoted fields keep commas, doubled quotes and newlines") {
    const auto t = parse_csv(
        "a,b\n"
        "\"x, y\",\"say \"\"hi\"\"\"\n"
        "\"two\nlines\",z\n"
        "after,multiline\n",
        "t.csv");
    REQUIRE(t.rows.size() == 3);
    CHECK(t.rows[0].fields[0] == "x, y");
    CHECK(t.rows[0].fields[1] == "say \"hi\"");
    CHECK(t.rows[1].fields[0] == "two\nlines");
    CHECK(t.rows[2].line == 5);
}

TEST_CASE("CRLF, BOM and missing final newline") {
    const auto t = parse_csv("\xef\xbb\xbfid,name\r\nS1,A\r\nS2,B", "t.csv");
    CHECK(t.header[0] == "id");
    REQUIRE(t.rows.size() == 2);
    CHECK(t.rows[0].fields[1] == "A");
    CHECK(t.rows[1].fields[1] == "B");
}

TEST_CASE("unquoted fields are trimmed, quoted fields are not, empties survive") {
    const auto t = parse_csv("a,b,c\n  x ,\" y \",\n", "t.csv");
    REQUIRE(t.rows.size() == 1);
    CHECK(t.rows[0].fields == std::vector<std::string>{"x", " y ", ""});
}

TEST_CASE("khmer fields are kept byte for byte") {
    const std::string central_market = "\xe1\x9e\x95\xe1\x9f\x92\xe1\x9e\x9f\xe1\x9e\xb6\xe1\x9e\x9a";
    const auto t = parse_csv("id,name_km\nS1," + central_market + "\n", "t.csv");
    CHECK(t.rows[0].fields[1] == central_market);
}

TEST_CASE("column lookup by header name") {
    const auto t = parse_csv("id, name ,lat\n", "t.csv");
    CHECK(t.column("id") == 0u);
    CHECK(t.column("name") == 1u);
    CHECK(t.column("lat") == 2u);
    CHECK_FALSE(t.column("lon"));
}

TEST_CASE("errors name the file and line") {
    CHECK(error_of("") == "t.csv:1: missing header row");
    CHECK(error_of("# only a comment\n") == "t.csv:1: missing header row");
    CHECK(error_of("a,a\n") == "t.csv:1: duplicate column 'a'");
    CHECK(error_of("a,b\nx,y\nz\n") == "t.csv:3: expected 2 fields, found 1");
    CHECK(error_of("a,b\nx,\"open\n") == "t.csv:2: unterminated quoted field");
    CHECK(error_of("a,b\nx,y\"z\n") == "t.csv:2: quote inside unquoted field; wrap the whole field in quotes");
    CHECK(error_of("a,b\nx,\"y\"z\n") == "t.csv:2: unexpected character after closing quote");
    CHECK(error_of("a,b\nx,y\nx,caf\xe9\n") ==
          "t.csv:3: not valid UTF-8; save the file as 'CSV UTF-8'");
}

TEST_CASE("read_csv_file reads from disk and reports missing files") {
    const auto path = std::filesystem::temp_directory_path() / "ppbus_test_read.csv";
    {
        std::ofstream out(path, std::ios::binary);
        out << "id,name\r\nS1,A\r\n";
    }
    const auto t = read_csv_file(path);
    CHECK(t.source == path.generic_u8string());
    REQUIRE(t.rows.size() == 1);
    CHECK(t.rows[0].fields[1] == "A");
    std::filesystem::remove(path);

    CHECK_THROWS_AS(read_csv_file(path), DataError);
}

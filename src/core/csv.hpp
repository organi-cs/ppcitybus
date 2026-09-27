#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace ppbus {

// what() is "source:line: message" so data errors point straight at the row to fix.
class CsvError : public std::runtime_error {
public:
    CsvError(const std::string& source, std::size_t line, const std::string& message);
};

struct CsvRow {
    std::size_t line = 0;  // line where the record starts
    std::vector<std::string> fields;
};

struct CsvTable {
    std::string source;
    std::vector<std::string> header;
    std::vector<CsvRow> rows;

    std::optional<std::size_t> column(std::string_view name) const;
};

// RFC 4180 with extras for hand-edited files: UTF-8 BOM and CRLF accepted,
// blank lines and lines starting with '#' skipped, unquoted fields trimmed.
// The first record is the header; every row must have the same field count.
CsvTable parse_csv(std::string_view text, std::string source);

CsvTable read_csv_file(const std::filesystem::path& path);

}  // namespace ppbus

#include "core/csv.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>

#include "core/utf8.hpp"

namespace ppbus {

namespace {

std::string format_error(const std::string& source, std::size_t line, const std::string& message) {
    if (line == 0) return source + ": " + message;
    return source + ":" + std::to_string(line) + ": " + message;
}

class Parser {
public:
    Parser(std::string_view text, const std::string& source) : text_(text), source_(source) {}

    CsvTable run() {
        CsvTable table;
        table.source = source_;
        bool have_header = false;
        while (!at_end()) {
            if (skip_blank_or_comment()) continue;
            const std::size_t record_line = line_;
            auto fields = read_record(record_line);
            if (!have_header) {
                for (std::size_t i = 0; i < fields.size(); ++i) {
                    if (std::find(fields.begin(), fields.begin() + static_cast<std::ptrdiff_t>(i), fields[i]) !=
                        fields.begin() + static_cast<std::ptrdiff_t>(i)) {
                        fail(record_line, "duplicate column '" + fields[i] + "'");
                    }
                }
                table.header = std::move(fields);
                have_header = true;
            } else {
                if (fields.size() != table.header.size()) {
                    fail(record_line, "expected " + std::to_string(table.header.size()) + " fields, found " +
                                          std::to_string(fields.size()));
                }
                table.rows.push_back({record_line, std::move(fields)});
            }
        }
        if (!have_header) fail(1, "missing header row");
        return table;
    }

private:
    [[noreturn]] void fail(std::size_t line, const std::string& message) const {
        throw CsvError(source_, line, message);
    }

    bool at_end() const { return pos_ >= text_.size(); }
    char peek() const { return text_[pos_]; }

    bool at_line_end() const {
        return at_end() || peek() == '\n' ||
               (peek() == '\r' && pos_ + 1 < text_.size() && text_[pos_ + 1] == '\n');
    }

    void consume_line_end() {
        if (!at_end() && peek() == '\r') ++pos_;
        if (!at_end() && peek() == '\n') {
            ++pos_;
            ++line_;
        }
    }

    void skip_spaces() {
        while (!at_end() && (peek() == ' ' || peek() == '\t')) ++pos_;
    }

    bool skip_blank_or_comment() {
        const std::size_t start = pos_;
        skip_spaces();
        if (at_line_end()) {
            consume_line_end();
            return true;
        }
        if (peek() == '#') {
            while (!at_line_end()) ++pos_;
            consume_line_end();
            return true;
        }
        pos_ = start;
        return false;
    }

    std::vector<std::string> read_record(std::size_t record_line) {
        std::vector<std::string> fields;
        while (true) {
            skip_spaces();
            if (!at_end() && peek() == '"') {
                fields.push_back(read_quoted(record_line));
            } else {
                fields.push_back(read_unquoted(record_line));
            }
            if (!at_end() && peek() == ',') {
                ++pos_;
                continue;
            }
            consume_line_end();
            return fields;
        }
    }

    std::string read_quoted(std::size_t record_line) {
        std::string value;
        ++pos_;
        while (true) {
            if (at_end()) fail(record_line, "unterminated quoted field");
            const char c = peek();
            if (c == '"') {
                if (pos_ + 1 < text_.size() && text_[pos_ + 1] == '"') {
                    value.push_back('"');
                    pos_ += 2;
                    continue;
                }
                ++pos_;
                break;
            }
            if (c == '\n') ++line_;
            value.push_back(c);
            ++pos_;
        }
        skip_spaces();
        if (!at_end() && peek() != ',' && !at_line_end()) {
            fail(record_line, "unexpected character after closing quote");
        }
        return value;
    }

    std::string read_unquoted(std::size_t record_line) {
        const std::size_t start = pos_;
        while (!at_line_end() && peek() != ',') {
            if (peek() == '"') fail(record_line, "quote inside unquoted field; wrap the whole field in quotes");
            ++pos_;
        }
        std::string_view value = text_.substr(start, pos_ - start);
        const auto last = value.find_last_not_of(" \t");
        return std::string(last == std::string_view::npos ? std::string_view{} : value.substr(0, last + 1));
    }

    std::string_view text_;
    const std::string& source_;
    std::size_t pos_ = 0;
    std::size_t line_ = 1;
};

}  // namespace

CsvError::CsvError(const std::string& source, std::size_t line, const std::string& message)
    : std::runtime_error(format_error(source, line, message)) {}

std::optional<std::size_t> CsvTable::column(std::string_view name) const {
    const auto it = std::find(header.begin(), header.end(), name);
    if (it == header.end()) return std::nullopt;
    return static_cast<std::size_t>(it - header.begin());
}

CsvTable parse_csv(std::string_view text, std::string source) {
    constexpr std::string_view kBom = "\xef\xbb\xbf";
    if (text.substr(0, kBom.size()) == kBom) text.remove_prefix(kBom.size());

    const std::size_t bad = find_invalid_utf8(text);
    if (bad != std::string_view::npos) {
        const auto line = 1 + static_cast<std::size_t>(std::count(text.begin(), text.begin() + static_cast<std::ptrdiff_t>(bad), '\n'));
        throw CsvError(source, line, "not valid UTF-8; save the file as 'CSV UTF-8'");
    }
    return Parser(text, source).run();
}

CsvTable read_csv_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw CsvError(path.u8string(), 0, "cannot open file");
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    return parse_csv(text, path.u8string());
}

}  // namespace ppbus

#include "core/config.hpp"

#include <charconv>
#include <cmath>
#include <fstream>
#include <iterator>
#include <set>
#include <type_traits>
#include <variant>

namespace ppbus {

namespace {

enum class Bound { Positive, NonNegative, AtLeastOne };

struct Setting {
    std::string_view key;
    std::variant<int Config::*, double Config::*> field;
    Bound bound;
};

constexpr Setting kSettings[] = {
    {"fare_khr", &Config::fare_khr, Bound::NonNegative},
    {"walk_speed_kmh", &Config::walk_speed_kmh, Bound::Positive},
    {"walk_radius_m", &Config::walk_radius_m, Bound::NonNegative},
    {"access_radius_m", &Config::access_radius_m, Bound::NonNegative},
    {"detour_factor", &Config::detour_factor, Bound::AtLeastOne},
    {"transfer_penalty_s", &Config::transfer_penalty_s, Bound::NonNegative},
    {"walk_weight", &Config::walk_weight, Bound::AtLeastOne},
    {"default_headway_min", &Config::default_headway_min, Bound::Positive},
    {"default_min_between_stops", &Config::default_min_between_stops, Bound::Positive},
};

std::string_view trim(std::string_view s) {
    const auto first = s.find_first_not_of(" \t\r");
    if (first == std::string_view::npos) return {};
    const auto last = s.find_last_not_of(" \t\r");
    return s.substr(first, last - first + 1);
}

template <typename T>
bool parse_number(std::string_view s, T& out) {
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    if (ec != std::errc{} || end != s.data() + s.size()) return false;
    if constexpr (std::is_floating_point_v<T>) return std::isfinite(out);
    return true;
}

const char* bound_violation(double value, Bound bound) {
    switch (bound) {
        case Bound::Positive: return value > 0 ? nullptr : "> 0";
        case Bound::NonNegative: return value >= 0 ? nullptr : ">= 0";
        case Bound::AtLeastOne: return value >= 1 ? nullptr : ">= 1";
    }
    return nullptr;
}

}  // namespace

Config parse_config(std::string_view text, const std::string& source) {
    constexpr std::string_view kBom = "\xef\xbb\xbf";
    if (text.substr(0, kBom.size()) == kBom) text.remove_prefix(kBom.size());

    Config config;
    std::set<std::string_view> seen;
    std::size_t line_no = 0;
    while (!text.empty()) {
        ++line_no;
        const auto nl = text.find('\n');
        const std::string_view line = trim(text.substr(0, nl));
        text = nl == std::string_view::npos ? std::string_view{} : text.substr(nl + 1);
        if (line.empty() || line.front() == '#') continue;

        const auto eq = line.find('=');
        if (eq == std::string_view::npos) throw DataError(source, line_no, "expected key=value");
        const std::string_view key = trim(line.substr(0, eq));
        const std::string_view value = trim(line.substr(eq + 1));

        const Setting* setting = nullptr;
        for (const auto& s : kSettings) {
            if (s.key == key) setting = &s;
        }
        if (!setting) throw DataError(source, line_no, "unknown setting '" + std::string(key) + "'");
        if (!seen.insert(setting->key).second) {
            throw DataError(source, line_no, "'" + std::string(key) + "' is set twice");
        }

        double as_double = 0;
        bool ok = false;
        if (const auto* int_field = std::get_if<int Config::*>(&setting->field)) {
            int v = 0;
            ok = parse_number(value, v);
            config.*(*int_field) = v;
            as_double = v;
        } else {
            double v = 0;
            ok = parse_number(value, v);
            config.*std::get<double Config::*>(setting->field) = v;
            as_double = v;
        }
        if (!ok) {
            throw DataError(source, line_no,
                            "invalid value for '" + std::string(key) + "': '" + std::string(value) + "'");
        }
        if (const char* need = bound_violation(as_double, setting->bound)) {
            throw DataError(source, line_no, "'" + std::string(key) + "' must be " + need);
        }
    }
    return config;
}

Config load_config_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw DataError(path.u8string(), 0, "cannot open file");
    const std::string text{std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
    return parse_config(text, path.u8string());
}

}  // namespace ppbus

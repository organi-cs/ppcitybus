#include "core/utf8.hpp"

namespace ppbus {

namespace {

constexpr char32_t kReplacement = 0xFFFD;

struct Decoded {
    char32_t cp = kReplacement;
    std::size_t len = 1;
    bool ok = false;
};

Decoded decode_one(std::string_view s, std::size_t i) {
    const auto b0 = static_cast<unsigned char>(s[i]);
    if (b0 < 0x80) return {b0, 1, true};

    std::size_t len = 0;
    char32_t cp = 0;
    char32_t min = 0;
    if ((b0 & 0xE0) == 0xC0) {
        len = 2, cp = b0 & 0x1F, min = 0x80;
    } else if ((b0 & 0xF0) == 0xE0) {
        len = 3, cp = b0 & 0x0F, min = 0x800;
    } else if ((b0 & 0xF8) == 0xF0) {
        len = 4, cp = b0 & 0x07, min = 0x10000;
    } else {
        return {};
    }
    if (i + len > s.size()) return {};
    for (std::size_t k = 1; k < len; ++k) {
        const auto b = static_cast<unsigned char>(s[i + k]);
        if ((b & 0xC0) != 0x80) return {};
        cp = (cp << 6) | (b & 0x3F);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return {};
    return {cp, len, true};
}

}  // namespace

std::u32string decode_utf8(std::string_view bytes) {
    std::u32string out;
    out.reserve(bytes.size());
    for (std::size_t i = 0; i < bytes.size();) {
        const Decoded d = decode_one(bytes, i);
        out.push_back(d.cp);
        i += d.len;
    }
    return out;
}

std::string encode_utf8(std::u32string_view code_points) {
    std::string out;
    out.reserve(code_points.size());
    for (char32_t cp : code_points) {
        if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) cp = kReplacement;
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

std::size_t find_invalid_utf8(std::string_view bytes) {
    for (std::size_t i = 0; i < bytes.size();) {
        const Decoded d = decode_one(bytes, i);
        if (!d.ok) return i;
        i += d.len;
    }
    return std::string_view::npos;
}

}  // namespace ppbus

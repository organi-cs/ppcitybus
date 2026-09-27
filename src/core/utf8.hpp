#pragma once

#include <string>
#include <string_view>

namespace ppbus {

// Invalid sequences become U+FFFD, one per offending byte.
std::u32string decode_utf8(std::string_view bytes);

std::string encode_utf8(std::u32string_view code_points);

// Offset of the first byte that is not part of a valid UTF-8 sequence, or npos.
std::size_t find_invalid_utf8(std::string_view bytes);

}  // namespace ppbus

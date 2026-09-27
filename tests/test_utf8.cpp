#include <doctest.h>

#include "core/utf8.hpp"

using ppbus::decode_utf8;
using ppbus::encode_utf8;
using ppbus::find_invalid_utf8;

namespace {
// "ផ្សារ" (market): PHA, COENG, SA, AA, RO.
const std::string kPhsar = "\xe1\x9e\x95\xe1\x9f\x92\xe1\x9e\x9f\xe1\x9e\xb6\xe1\x9e\x9a";
}  // namespace

TEST_CASE("decode ascii and khmer to code points") {
    CHECK(decode_utf8("Bus 5") == U"Bus 5");
    CHECK(decode_utf8(kPhsar) == std::u32string{0x1795, 0x17D2, 0x179F, 0x17B6, 0x179A});
    CHECK(decode_utf8("").empty());
}

TEST_CASE("encode round-trips every sequence length") {
    const std::u32string cps{U'A', 0x00E9, 0x1795, 0x1F68C};  // 1, 2, 3 and 4 bytes
    const std::string bytes = encode_utf8(cps);
    CHECK(bytes == "A\xc3\xa9\xe1\x9e\x95\xf0\x9f\x9a\x8c");
    CHECK(decode_utf8(bytes) == cps);
    CHECK(encode_utf8(decode_utf8(kPhsar)) == kPhsar);
}

TEST_CASE("invalid bytes decode to replacement characters") {
    CHECK(decode_utf8("\xc3") == std::u32string{0xFFFD});             // truncated
    CHECK(decode_utf8("a\xff" "b") == std::u32string{U'a', 0xFFFD, U'b'});
    CHECK(decode_utf8("\xc0\xaf").find(0xFFFD) != std::u32string::npos);  // overlong '/'
    CHECK(decode_utf8("\xed\xa0\x80").find(0xFFFD) != std::u32string::npos);  // surrogate
    CHECK(decode_utf8("\xf4\x90\x80\x80").find(0xFFFD) != std::u32string::npos);  // above U+10FFFF
}

TEST_CASE("find_invalid_utf8 reports the first bad byte offset") {
    CHECK(find_invalid_utf8("plain ascii") == std::string_view::npos);
    CHECK(find_invalid_utf8(kPhsar) == std::string_view::npos);
    CHECK(find_invalid_utf8("ok\x96" "bad") == 2);  // cp1252 en dash, as saved by Excel
    CHECK(find_invalid_utf8(kPhsar + "\xe1\x9e") == kPhsar.size());
}

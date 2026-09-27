#include <doctest.h>

#include "core/version.hpp"

TEST_CASE("version is a dotted triple") {
    const auto v = ppbus::version();
    CHECK(v.size() >= 5);
    CHECK(v.find('.') != std::string_view::npos);
}

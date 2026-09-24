#include <catch2/catch_test_macros.hpp>
#include <leon/core/Ascii.h>

TEST_CASE("AsciiToLower lowercases ASCII letters", "[core][ascii]") {
    REQUIRE(leon::AsciiToLower("AbC") == "abc");
    REQUIRE(leon::AsciiToLower("Cube") == "cube");
    REQUIRE(leon::AsciiToLower("123_OK") == "123_ok");
    REQUIRE(leon::AsciiToLower("") == "");
}

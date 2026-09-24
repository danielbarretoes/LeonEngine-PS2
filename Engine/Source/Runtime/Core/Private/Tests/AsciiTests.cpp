#include <catch2/catch_test_macros.hpp>
#include "Misc/Ascii.h"

TEST_CASE("AsciiToLower lowercases ASCII letters", "[core][ascii]") {
    REQUIRE(AsciiToLower("AbC") == "abc");
    REQUIRE(AsciiToLower("Cube") == "cube");
    REQUIRE(AsciiToLower("123_OK") == "123_ok");
    REQUIRE(AsciiToLower("") == "");
}

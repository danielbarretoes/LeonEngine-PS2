#include <catch2/catch_test_macros.hpp>
#include "Misc/CString.h"

TEST_CASE("AsciiToLower lowercases ASCII letters", "[core][ascii]") {
    REQUIRE(FCString::ToLower("AbC") == "abc");
    REQUIRE(FCString::ToLower("Cube") == "cube");
    REQUIRE(FCString::ToLower("123_OK") == "123_ok");
    REQUIRE(FCString::ToLower("") == "");
}

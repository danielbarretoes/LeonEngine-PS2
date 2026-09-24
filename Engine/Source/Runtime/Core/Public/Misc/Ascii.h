#pragma once

#include <cctype>
#include <string>
#include <string_view>


/// ASCII lower-case copy (for case-insensitive JSON class / primitive names).
[[nodiscard]] inline std::string AsciiToLower(std::string_view text) {
    std::string out(text);
    for (char& c : out) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return out;
}


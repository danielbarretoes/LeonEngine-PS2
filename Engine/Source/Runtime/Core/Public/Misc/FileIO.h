#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>


/// Write bytes via a same-directory temp file + rename (crash-safe vs trunc-in-place).
[[nodiscard]] bool WriteFileAtomic(const std::filesystem::path& path, const void* data,
                                   std::size_t size);

[[nodiscard]] bool WriteFileAtomic(const std::filesystem::path& path,
                                   const std::vector<std::uint8_t>& bytes);

[[nodiscard]] bool WriteTextFileAtomic(const std::filesystem::path& path, std::string_view text);


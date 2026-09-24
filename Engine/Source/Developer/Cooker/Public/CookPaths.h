#pragma once

#include <filesystem>
#include <string>


/// Resolve `rel` against `baseDir` (absolute paths unchanged).
[[nodiscard]] std::string ResolveBeside(const std::filesystem::path& baseDir,
                                        const std::string& rel);


#pragma once

#include <filesystem>
#include <string>

namespace leon::tools {

/// Resolve `rel` against `baseDir` (absolute paths unchanged).
[[nodiscard]] std::string ResolveBeside(const std::filesystem::path& baseDir,
                                        const std::string& rel);

} // namespace leon::tools

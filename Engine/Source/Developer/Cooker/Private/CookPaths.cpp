#include "CookPaths.h"

namespace leon::tools {

std::string ResolveBeside(const std::filesystem::path& baseDir, const std::string& rel) {
    const std::filesystem::path p(rel);
    if (p.is_absolute()) {
        return p.lexically_normal().string();
    }
    return (baseDir / p).lexically_normal().string();
}

} // namespace leon::tools

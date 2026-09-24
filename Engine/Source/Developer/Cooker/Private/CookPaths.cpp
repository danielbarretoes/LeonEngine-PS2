#include "CookPaths.h"


std::string ResolveBeside(const std::filesystem::path& baseDir, const std::string& rel) {
    const std::filesystem::path p(rel);
    if (p.is_absolute()) {
        return p.lexically_normal().string();
    }
    return (baseDir / p).lexically_normal().string();
}


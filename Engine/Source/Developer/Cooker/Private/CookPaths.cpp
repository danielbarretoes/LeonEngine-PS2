#include "CookPaths.h"


std::string FCookPaths::ResolveBeside(const std::filesystem::path& BaseDir, const std::string& Rel) {
    const std::filesystem::path P(Rel);
    if (P.is_absolute()) {
        return P.lexically_normal().string();
    }
    return (BaseDir / P).lexically_normal().string();
}


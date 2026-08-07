#include <leon/core/Paths.h>

namespace leon {
namespace {

std::filesystem::path g_activeContentRoot{};
std::filesystem::path g_exeDir{"host:"};

} // namespace

std::filesystem::path ExecutableDirectory() {
    return g_exeDir;
}

void SetActiveContentRoot(const std::filesystem::path& projectOrPackRoot) {
    g_activeContentRoot = projectOrPackRoot;
}

std::filesystem::path ActiveContentRoot() {
    return g_activeContentRoot;
}

std::string ResolveAssetPath(const std::string& relativePath) {
    if (relativePath.empty()) {
        return {};
    }
    if (!g_activeContentRoot.empty()) {
        return (g_activeContentRoot / relativePath).string();
    }
    return (g_exeDir / "assets" / relativePath).string();
}

std::string ResolveProjectsDirectory() {
    return (g_exeDir / "Projects").string();
}

std::filesystem::path ProjectContentDirectory(const std::filesystem::path& projectOrPackRoot) {
    return projectOrPackRoot / "Content";
}

std::string ResolveContentAssetPath(const std::filesystem::path& projectOrPackRoot,
                                    const std::string& relativeOrKey) {
    if (relativeOrKey.empty()) {
        return {};
    }
    return (ProjectContentDirectory(projectOrPackRoot) / relativeOrKey).string();
}

} // namespace leon

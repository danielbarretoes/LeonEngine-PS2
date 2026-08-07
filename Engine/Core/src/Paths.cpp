#include <array>
#include <filesystem>
#include <initializer_list>
#include <leon/core/Paths.h>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace leon {
namespace {

std::filesystem::path& ActiveContentRootStorage() {
    static std::filesystem::path root;
    return root;
}

/// Prefer the newest existing candidate so repo edits win over a stale POST_BUILD copy.
std::string newestExisting(const std::vector<std::filesystem::path>& candidates) {
    std::filesystem::path best;
    std::filesystem::file_time_type bestTime{};
    bool found = false;
    for (const auto& path : candidates) {
        std::error_code ec;
        if (!std::filesystem::exists(path, ec) || ec) {
            continue;
        }
        const auto time = std::filesystem::last_write_time(path, ec);
        if (ec) {
            continue;
        }
        if (!found || time > bestTime) {
            best = path;
            bestTime = time;
            found = true;
        }
    }
    if (!found) {
        return {};
    }
    return best.lexically_normal().string();
}

std::string newestExisting(std::initializer_list<std::filesystem::path> candidates) {
    return newestExisting(std::vector<std::filesystem::path>(candidates));
}

std::filesystem::path stripAssetsPrefix(const std::filesystem::path& rel) {
    const std::string s = rel.generic_string();
    if (s.starts_with("assets/")) {
        return s.substr(7);
    }
    if (s == "assets") {
        return {};
    }
    return rel;
}

void appendActivePackCandidates(std::vector<std::filesystem::path>& out,
                                const std::filesystem::path& rel,
                                const std::filesystem::path& underAssets) {
    const std::filesystem::path active = ActiveContentRootStorage();
    if (active.empty()) {
        return;
    }
    const std::filesystem::path content = ProjectContentDirectory(active);
    if (!content.empty()) {
        out.push_back(content / rel);
        if (!underAssets.empty()) {
            out.push_back(content / "assets" / underAssets);
            out.push_back(content / underAssets);
        }
    }
    out.push_back(active / rel);
    if (!underAssets.empty()) {
        out.push_back(active / "assets" / underAssets);
    }
}

} // namespace

void SetActiveContentRoot(const std::filesystem::path& projectOrPackRoot) {
    std::error_code ec;
    ActiveContentRootStorage() =
        projectOrPackRoot.empty() ? std::filesystem::path{}
                                  : projectOrPackRoot.lexically_normal();
    (void)ec;
}

std::filesystem::path ActiveContentRoot() {
    return ActiveContentRootStorage();
}

std::filesystem::path ExecutableDirectory() {
#if defined(_WIN32)
    std::vector<wchar_t> buffer(MAX_PATH);
    for (;;) {
        const DWORD length =
            GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length == 0) {
            return std::filesystem::current_path();
        }
        if (length < buffer.size()) {
            return std::filesystem::path(buffer.data()).parent_path();
        }
        // Truncated — grow and retry (long paths / Unicode).
        buffer.resize(buffer.size() * 2);
        if (buffer.size() > 32768) {
            return std::filesystem::current_path();
        }
    }
#else
    std::error_code ec;
    auto path = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        return path.parent_path();
    }
    return std::filesystem::current_path();
#endif
}

std::filesystem::path ProjectContentDirectory(const std::filesystem::path& projectOrPackRoot) {
    std::error_code ec;
    const std::filesystem::path root = projectOrPackRoot.lexically_normal();
    if (root.empty()) {
        return {};
    }
    const std::filesystem::path content = root / "Content";
    const bool contentLevels = std::filesystem::is_directory(content / "Levels", ec) && !ec;
    const bool legacyLevels = std::filesystem::is_directory(root / "Levels", ec) && !ec;
    if (contentLevels) {
        return content;
    }
    if (legacyLevels) {
        return root;
    }
    // Default Unreal-like layout (folder may be created by the editor).
    return content;
}

std::string ResolveContentAssetPath(const std::filesystem::path& projectOrPackRoot,
                                    const std::string& relativeOrKey) {
    std::error_code ec;
    if (relativeOrKey.empty()) {
        return {};
    }
    const std::filesystem::path key(relativeOrKey);
    if (key.is_absolute() && std::filesystem::exists(key, ec) && !ec) {
        return key.lexically_normal().string();
    }

    const std::filesystem::path content = ProjectContentDirectory(projectOrPackRoot);
    if (!content.empty()) {
        const std::filesystem::path inContent = (content / key).lexically_normal();
        if (std::filesystem::exists(inContent, ec) && !ec) {
            return inContent.string();
        }
    }

    if (!projectOrPackRoot.empty()) {
        const std::filesystem::path inRoot = (projectOrPackRoot / key).lexically_normal();
        if (std::filesystem::exists(inRoot, ec) && !ec) {
            return inRoot.string();
        }
    }

    // Engine / exe staging only — never another project's Content/.
    return ResolveAssetPath(relativeOrKey);
}

std::string ResolveAssetPath(const std::string& relativePath) {
    const std::filesystem::path exeDir = ExecutableDirectory();
    const std::filesystem::path rel(relativePath);
    const std::filesystem::path underAssets = stripAssetsPrefix(rel);

    // Active pack Content wins outright (never lose to a newer Engine/staging copy).
    {
        std::vector<std::filesystem::path> packCandidates;
        appendActivePackCandidates(packCandidates, rel, underAssets);
        const std::string packHit = newestExisting(packCandidates);
        if (!packHit.empty()) {
            return packHit;
        }
    }

    std::vector<std::filesystem::path> candidates = {
        exeDir / rel,
        exeDir / "assets" / underAssets,
        std::filesystem::path("assets") / underAssets,
        rel,
        std::filesystem::path("../") / rel,
        std::filesystem::path("../../") / rel,
        std::filesystem::path("../../../") / rel,
        exeDir / ".." / rel,
        exeDir / "../.." / rel,
        exeDir / "../../.." / rel,
        std::filesystem::path("Engine") / "Assets" / underAssets,
        std::filesystem::path("../Engine") / "Assets" / underAssets,
        std::filesystem::path("../../Engine") / "Assets" / underAssets,
        std::filesystem::path("../../../Engine") / "Assets" / underAssets,
        exeDir / "Engine" / "Assets" / underAssets,
        exeDir / ".." / "Engine" / "Assets" / underAssets,
        exeDir / "../.." / "Engine" / "Assets" / underAssets,
        exeDir / "../../.." / "Engine" / "Assets" / underAssets,
        // Shared ThirdPerson character content (not a game pack).
        std::filesystem::path("Templates") / "ThirdPerson" / "Content" / "assets" / underAssets,
        std::filesystem::path("../Templates") / "ThirdPerson" / "Content" / "assets" / underAssets,
        std::filesystem::path("../../Templates") / "ThirdPerson" / "Content" / "assets" /
            underAssets,
        std::filesystem::path("../../../Templates") / "ThirdPerson" / "Content" / "assets" /
            underAssets,
        exeDir / "Templates" / "ThirdPerson" / "Content" / "assets" / underAssets,
        exeDir / ".." / "Templates" / "ThirdPerson" / "Content" / "assets" / underAssets,
        exeDir / "../.." / "Templates" / "ThirdPerson" / "Content" / "assets" / underAssets,
        exeDir / "../../.." / "Templates" / "ThirdPerson" / "Content" / "assets" / underAssets,
    };

    std::string found = newestExisting(candidates);
    if (!found.empty()) {
        return found;
    }
    return (exeDir / rel).lexically_normal().string();
}

std::string ResolveProjectsDirectory() {
    // Prefer a Projects/ whose parent is a real Leon root (Build/Dependencies.cmake +
    // Engine/). Avoid the thin POST_BUILD staging folder beside the editor exe
    // (e.g. Editor/build/Release/Projects) — that breaks game CMakeLists ../.. paths.
    const std::filesystem::path exeDir = ExecutableDirectory();
    std::error_code ec;
    const auto isSdkProjects = [&](const std::filesystem::path& projectsDir) {
        if (!std::filesystem::is_directory(projectsDir, ec) || ec) {
            return false;
        }
        const std::filesystem::path root = projectsDir.parent_path();
        return std::filesystem::is_regular_file(root / "Build" / "Dependencies.cmake", ec) &&
               !ec && std::filesystem::is_directory(root / "Engine", ec) && !ec;
    };

    const std::filesystem::path candidates[] = {
        exeDir / "Projects",                      // Dist/LeonEditor/Projects
        exeDir / ".." / ".." / "Projects",        // Editor/build-fast → repo/Projects
        exeDir / ".." / ".." / ".." / "Projects", // Editor/build/Release → repo/Projects
        std::filesystem::path("Projects"),
        std::filesystem::path("..") / "Projects",
        std::filesystem::path("..") / ".." / "Projects",
        std::filesystem::path("..") / ".." / ".." / "Projects",
    };
    for (const auto& c : candidates) {
        const std::filesystem::path p = c.lexically_normal();
        if (isSdkProjects(p)) {
            return p.string();
        }
    }

    // Fallback without calling ResolveAssetPath("Projects") — that would recurse via Projects scan.
    if (std::filesystem::is_directory(exeDir / "Projects", ec) && !ec) {
        return (exeDir / "Projects").lexically_normal().string();
    }
    if (std::filesystem::is_directory("Projects", ec) && !ec) {
        return std::filesystem::path("Projects").lexically_normal().string();
    }
    return (exeDir / "Projects").lexically_normal().string();
}

} // namespace leon

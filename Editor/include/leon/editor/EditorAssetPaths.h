#pragma once

#include <filesystem>
#include <iostream>
#include <leon/core/Paths.h>
#include <leon/editor/EditorContext.h>
#include <leon/level/Level.h>
#include <string>

namespace leon::editor {
namespace detail {

[[nodiscard]] inline bool PathLooksRelative(const std::filesystem::path& p) {
    return p.root_name().empty() && !p.is_absolute();
}

[[nodiscard]] inline bool IsUnderRoot(const std::filesystem::path& path,
                                      const std::filesystem::path& root,
                                      std::filesystem::path& outRelative) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (root.empty() || path.empty()) {
        return false;
    }
    const fs::path canonRoot = fs::weakly_canonical(root, ec);
    if (ec || canonRoot.empty()) {
        return false;
    }
    const fs::path canonPath = fs::weakly_canonical(path, ec);
    if (ec || canonPath.empty()) {
        return false;
    }
    const fs::path rel = canonPath.lexically_relative(canonRoot);
    if (rel.empty()) {
        return false;
    }
    for (const fs::path& part : rel) {
        if (part == "..") {
            return false;
        }
    }
    outRelative = rel;
    return true;
}

[[nodiscard]] inline std::filesystem::path EngineAssetsRootHint() {
    namespace fs = std::filesystem;
    std::error_code ec;
    const std::string resolved = ResolveAssetPath(".");
    if (!resolved.empty()) {
        const fs::path p = fs::weakly_canonical(resolved, ec);
        if (!ec && !p.empty()) {
            return p;
        }
    }
    const fs::path exe = ExecutableDirectory();
    const fs::path candidates[] = {
        exe / "assets",
        exe / ".." / ".." / "Engine" / "Assets",
        exe / ".." / ".." / ".." / "Engine" / "Assets",
        fs::path("Engine") / "Assets",
    };
    for (const fs::path& c : candidates) {
        if (fs::is_directory(c, ec) && !ec) {
            return fs::weakly_canonical(c, ec);
        }
    }
    return {};
}

} // namespace detail

/// Prefer pack-/content-relative keys for level authoring (stable across machines).
[[nodiscard]] inline std::string MakePackRelativeAssetPath(const EditorContext& ctx,
                                                           const std::string& inPath) {
    namespace fs = std::filesystem;
    if (inPath.empty()) {
        return {};
    }

    const fs::path input(inPath);
    if (detail::PathLooksRelative(input)) {
        return input.lexically_normal().generic_string();
    }

    fs::path relative;
    if (!ctx.projectPath.empty()) {
        const fs::path content = ProjectContentDirectory(ctx.projectPath);
        if (detail::IsUnderRoot(input, content, relative)) {
            return relative.generic_string();
        }
    }

    {
        const fs::path active = ActiveContentRoot();
        if (!active.empty()) {
            const fs::path content = ProjectContentDirectory(active);
            if (!content.empty() && detail::IsUnderRoot(input, content, relative)) {
                return relative.generic_string();
            }
            if (detail::IsUnderRoot(input, active, relative)) {
                return relative.generic_string();
            }
        }
    }

    {
        const fs::path engineRoot = detail::EngineAssetsRootHint();
        if (!engineRoot.empty() && detail::IsUnderRoot(input, engineRoot, relative)) {
            return relative.generic_string();
        }
    }

    static bool warned = false;
    if (!warned) {
        warned = true;
        std::cerr << "Editor: could not relativize asset path (keeping absolute): " << inPath
                  << '\n';
    }
    return inPath;
}

inline void RemapLevelPathsToContentRelative(EditorContext& ctx) {
    if (ctx.level == nullptr) {
        return;
    }
    Level& level = *ctx.level;
    for (StaticMeshComponent& mesh : level.StaticMeshes()) {
        if (!mesh.meshPath.empty()) {
            mesh.meshPath = MakePackRelativeAssetPath(ctx, mesh.meshPath);
        }
        if (!mesh.materialPath.empty()) {
            mesh.materialPath = MakePackRelativeAssetPath(ctx, mesh.materialPath);
        }
        if (!mesh.lightmapPath.empty()) {
            mesh.lightmapPath = MakePackRelativeAssetPath(ctx, mesh.lightmapPath);
        }
    }
    if (!level.EnvironmentPath().empty()) {
        level.SetEnvironmentPath(MakePackRelativeAssetPath(ctx, level.EnvironmentPath()));
    }
}

} // namespace leon::editor

#pragma once

#include <filesystem>
#include <string>


[[nodiscard]] std::filesystem::path ExecutableDirectory();

/// Pin asset resolution to a project/pack root (`Projects/<name>/`).
/// When set, `ResolveAssetPath` prefers that pack's Content/ before Engine assets.
/// Cleared with an empty path.
void SetActiveContentRoot(const std::filesystem::path& projectOrPackRoot);

[[nodiscard]] std::filesystem::path ActiveContentRoot();

/// Resolve a path relative to the executable / Engine Assets / active pack Content.
/// When several Engine/staging candidates exist, picks the newest (repo edits beat a
/// stale POST_BUILD copy). Does **not** scan unrelated projects.
[[nodiscard]] std::string ResolveAssetPath(const std::string& relativePath);

/// Resolve the projects root (`Projects/`).
[[nodiscard]] std::string ResolveProjectsDirectory();

/// Unreal-like project Content folder: `<project>/Content`.
/// If `<project>/Levels` exists without `Content/Levels` (old pack layout), returns `<project>`.
[[nodiscard]] std::filesystem::path ProjectContentDirectory(
    const std::filesystem::path& projectOrPackRoot);

/// Resolve a content-relative key (`Materials/M_Floor.lmat`) under a project's Content/.
/// Falls back to Engine assets via `ResolveAssetPath` (not other packs).
[[nodiscard]] std::string ResolveContentAssetPath(const std::filesystem::path& projectOrPackRoot,
                                                  const std::string& relativeOrKey);


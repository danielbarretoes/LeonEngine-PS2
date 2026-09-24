#pragma once

#include "CoreTypes.h"

#include <filesystem>
#include <string>

/** Engine / project path resolution (UE: FPaths). */
struct CORE_API FPaths
{
	/** Folder of the running executable. */
	[[nodiscard]] static std::filesystem::path ExecutableDir();

	/**
	 * Pins asset resolution to a project root: ResolveAssetPath then prefers that project's Content/
	 * before Engine content. Cleared with an empty path.
	 */
	static void SetActiveContentRoot(const std::filesystem::path& ProjectRoot);

	[[nodiscard]] static std::filesystem::path GetActiveContentRoot();

	/**
	 * Resolves a path relative to the executable / Engine/Content / Engine/Shaders / the active
	 * project Content. When several Engine or staging candidates exist the newest wins (repo edits beat
	 * a stale post-build copy). Does not scan unrelated projects.
	 */
	[[nodiscard]] static std::string ResolveAssetPath(const std::string& RelativePath);

	/** The folder holding runtime project packs (`Projects/`). */
	[[nodiscard]] static std::string ResolveProjectsDir();

	/**
	 * Project Content folder (UE: ProjectContentDir): `<project>/Content`. An old pack layout with
	 * `<project>/Levels` and no `Content/Levels` returns `<project>`.
	 */
	[[nodiscard]] static std::filesystem::path ProjectContentDir(const std::filesystem::path& ProjectRoot);

	/**
	 * Resolves a content-relative key (`Materials/M_Floor.lmat`) under a project's Content/, falling back
	 * to Engine content through ResolveAssetPath (never other projects).
	 */
	[[nodiscard]] static std::string ResolveContentAssetPath(
		const std::filesystem::path& ProjectRoot, const std::string& RelativeOrKey);
};

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
	 * Resolves a path relative to the executable / Engine/Content / Engine/Shaders. When several Engine or staging
	 * candidates exist the newest wins (repo edits beat a stale post-build copy). Does not scan unrelated projects.
	 */
	[[nodiscard]] static std::string ResolveAssetPath(const std::string& RelativePath);
};

#pragma once

#include "CoreTypes.h"

#include <filesystem>
#include <string>

/** Path helpers for cook recipes. */
struct COOKER_API FCookPaths
{
	/** Resolves Relative against BaseDir (absolute paths are returned unchanged). */
	[[nodiscard]] static std::string ResolveBeside(const std::filesystem::path& BaseDir, const std::string& Relative);
};

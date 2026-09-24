#pragma once

#include "CoreTypes.h"

#include <string>

/** Cook recipe runner: a JSON `steps` array of `staticmesh` steps. */
struct COOKER_API FCookRecipe
{
	/** Runs a recipe file; relative paths resolve next to it. Returns a process exit code (0 ok). */
	[[nodiscard]] static int32 RunFile(const std::string& RecipePath);
};

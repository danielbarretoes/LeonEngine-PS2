#pragma once

#include "CoreMinimal.h"

/** Path helpers for cook recipes. */
struct COOKER_API FCookPaths
{
	/** Resolves Relative against BaseDir (absolute paths are only normalized). */
	[[nodiscard]] static FString ResolveBeside(const FString& BaseDir, const FString& Relative);
};

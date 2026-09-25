#pragma once

// std::string bridge to FPaths::ResolveLegacyContentPath for the desktop modules that still use std::string (until
// P6). The legacy content keys themselves go away with the old formats in P15.

#include "Containers/UnrealString.h"
#include "Misc/Paths.h"

#include <string>

/** Absolute path of a legacy content key ("assets/Shaders/x.vert", "LevelTemplates/Starter.llev"). */
inline std::string ResolveLegacyContentPath(const std::string& RelativePath)
{
	return std::string(*FPaths::ResolveLegacyContentPath(FString(RelativePath.c_str())));
}

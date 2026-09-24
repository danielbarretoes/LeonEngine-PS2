#pragma once

#include "HAL/PreprocessorHelpers.h"

// Engine version from Engine/Build/Build.version (LeonBuildTool defines ENGINE_{MAJOR,MINOR,PATCH}_VERSION).
#ifndef ENGINE_MAJOR_VERSION
	#define ENGINE_MAJOR_VERSION 0
#endif
#ifndef ENGINE_MINOR_VERSION
	#define ENGINE_MINOR_VERSION 0
#endif
#ifndef ENGINE_PATCH_VERSION
	#define ENGINE_PATCH_VERSION 0
#endif

/** "Major.Minor.Patch" string literal. */
#define LEON_ENGINE_VERSION_STRING                                                                                     \
	PREPROCESSOR_TO_STRING(ENGINE_MAJOR_VERSION)                                                                       \
	"." PREPROCESSOR_TO_STRING(ENGINE_MINOR_VERSION) "." PREPROCESSOR_TO_STRING(ENGINE_PATCH_VERSION)

/** Engine version for logs and window titles (e.g. "0.12.0"). */
[[nodiscard]] inline constexpr const char* EngineVersionString()
{
	return LEON_ENGINE_VERSION_STRING;
}

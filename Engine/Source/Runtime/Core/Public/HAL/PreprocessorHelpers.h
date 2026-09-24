#pragma once

/** Turns the preprocessor token argument into a string (expanding macros first). */
#define PREPROCESSOR_TO_STRING(x) PREPROCESSOR_TO_STRING_INNER(x)
#define PREPROCESSOR_TO_STRING_INNER(x) #x

/** Concatenates two preprocessor tokens (expanding macros first). */
#define PREPROCESSOR_JOIN(x, y) PREPROCESSOR_JOIN_INNER(x, y)
#define PREPROCESSOR_JOIN_INNER(x, y) x##y

/** Platform header folder name; LeonBuildTool defines LBT_COMPILED_PLATFORM (UE: UBT_COMPILED_PLATFORM). */
#ifdef OVERRIDE_PLATFORM_HEADER_NAME
	#define PLATFORM_HEADER_NAME OVERRIDE_PLATFORM_HEADER_NAME
#else
	#define PLATFORM_HEADER_NAME LBT_COMPILED_PLATFORM
#endif

/** Platform extensions (Engine/Platforms/<Platform>) keep their headers at the root of Public/. */
#ifndef PLATFORM_IS_EXTENSION
	#define PLATFORM_IS_EXTENSION 0
#endif

/**
 * Includes the current platform's version of a header:
 *   #include COMPILED_PLATFORM_HEADER(PlatformMemory.h)
 * resolves to "Windows/WindowsPlatformMemory.h" in-module, or "PS2PlatformMemory.h" for an extension.
 */
// clang-format off: the path separator must stay glued to its tokens ("Windows/WindowsPlatform.h").
#if PLATFORM_IS_EXTENSION
	#define COMPILED_PLATFORM_HEADER(Suffix) PREPROCESSOR_TO_STRING(PREPROCESSOR_JOIN(PLATFORM_HEADER_NAME, Suffix))
#else
	#define COMPILED_PLATFORM_HEADER(Suffix) PREPROCESSOR_TO_STRING(PREPROCESSOR_JOIN(PLATFORM_HEADER_NAME/PLATFORM_HEADER_NAME, Suffix))
#endif
// clang-format on

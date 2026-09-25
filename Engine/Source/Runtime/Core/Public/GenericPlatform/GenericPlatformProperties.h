#pragma once

#include "HAL/Platform.h"

/** Compile-time platform properties; each platform overrides what differs (UE: FGenericPlatformProperties). */
struct CORE_API FGenericPlatformProperties
{
	/** Platform name as used by LeonBuildTool (Win64, Linux, PS2). */
	static FORCEINLINE const char* PlatformName()
	{
		return "";
	}

	/** Platform name in config paths: Engine/Config/<Name>/<Name>Engine.ini (UE: IniPlatformName). */
	static FORCEINLINE const char* IniPlatformName()
	{
		return "";
	}

	/** True for platforms that only ever run the game (no tools, no cooking). */
	static FORCEINLINE bool IsGameOnly()
	{
		return false;
	}

	/** True when the platform has a desktop windowing system. */
	static FORCEINLINE bool HasWindowingSystem()
	{
		return PLATFORM_DESKTOP != 0;
	}

	/** FName pool: bytes per block, block count limit (exceeding it is a fatal error) and hash buckets (Leon). */
	static constexpr uint32 NamePoolBlockSize = 64 * 1024;
	static constexpr uint32 NamePoolMaxBlocks = 1024;
	static constexpr uint32 NamePoolHashBuckets = 65536;
};

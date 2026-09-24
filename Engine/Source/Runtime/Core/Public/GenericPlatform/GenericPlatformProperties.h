#pragma once

#include "HAL/Platform.h"

/** Compile-time platform properties; each platform overrides what differs (UE: FGenericPlatformProperties). */
struct FGenericPlatformProperties
{
	/** Platform name as used by LeonBuildTool (Win64, Linux, PS2). */
	static FORCEINLINE const char* PlatformName()
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
};

#pragma once

#include "GenericPlatform/GenericPlatformProperties.h"

struct CORE_API FPS2PlatformProperties : public FGenericPlatformProperties
{
	static FORCEINLINE const char* PlatformName()
	{
		return "PS2";
	}

	static FORCEINLINE bool IsGameOnly()
	{
		return true;
	}

	/** FName pool within 32 MB: 16 KB blocks, at most 256 KB, 4096 buckets (16 KB). */
	static constexpr uint32 NamePoolBlockSize = 16 * 1024;
	static constexpr uint32 NamePoolMaxBlocks = 16;
	static constexpr uint32 NamePoolHashBuckets = 4096;
};

typedef FPS2PlatformProperties FPlatformProperties;

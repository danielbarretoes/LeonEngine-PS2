#pragma once

#include "GenericPlatform/GenericPlatformProperties.h"

struct FPS2PlatformProperties : public FGenericPlatformProperties
{
	static FORCEINLINE const char* PlatformName()
	{
		return "PS2";
	}

	static FORCEINLINE bool IsGameOnly()
	{
		return true;
	}
};

typedef FPS2PlatformProperties FPlatformProperties;

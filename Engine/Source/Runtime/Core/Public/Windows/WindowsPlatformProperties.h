#pragma once

#include "GenericPlatform/GenericPlatformProperties.h"

struct CORE_API FWindowsPlatformProperties : public FGenericPlatformProperties
{
	static FORCEINLINE const char* PlatformName()
	{
		return "Win64";
	}
};

typedef FWindowsPlatformProperties FPlatformProperties;

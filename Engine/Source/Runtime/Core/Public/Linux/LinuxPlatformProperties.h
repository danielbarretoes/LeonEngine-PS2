#pragma once

#include "GenericPlatform/GenericPlatformProperties.h"

struct CORE_API FLinuxPlatformProperties : public FGenericPlatformProperties
{
	static FORCEINLINE const char* PlatformName()
	{
		return "Linux";
	}
};

typedef FLinuxPlatformProperties FPlatformProperties;

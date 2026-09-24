#pragma once

#include "GenericPlatform/GenericPlatformProperties.h"

struct FLinuxPlatformProperties : public FGenericPlatformProperties
{
	static FORCEINLINE const char* PlatformName()
	{
		return "Linux";
	}
};

typedef FLinuxPlatformProperties FPlatformProperties;

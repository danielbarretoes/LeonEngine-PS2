#pragma once

#include "GenericPlatform/GenericPlatformMemory.h"

struct CORE_API FWindowsPlatformMemory : public FGenericPlatformMemory
{
	static FPlatformMemoryStats GetStats();
};

typedef FWindowsPlatformMemory FPlatformMemory;

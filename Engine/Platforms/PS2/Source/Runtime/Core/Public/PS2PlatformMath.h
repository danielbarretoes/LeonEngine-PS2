#pragma once

#include "GenericPlatform/GenericPlatformMath.h"

/** EE math: quarter-wave sine table instead of libm (soft-float double is slow on the EE). */
struct CORE_API FPS2PlatformMath : public FGenericPlatformMath
{
	static float Sin256(uint32 Angle256);
	static float Cos256(uint32 Angle256);
};

typedef FPS2PlatformMath FPlatformMath;

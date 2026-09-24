#pragma once

#include "HAL/Platform.h"

/** Math helpers with platform overrides (UE: FGenericPlatformMath, reduced). */
struct CORE_API FGenericPlatformMath
{
	/** Sine / cosine of an angle in 1/256 turns (Leon extension; PS2 uses a table, no libm). */
	static float Sin256(uint32 Angle256);
	static float Cos256(uint32 Angle256);
};

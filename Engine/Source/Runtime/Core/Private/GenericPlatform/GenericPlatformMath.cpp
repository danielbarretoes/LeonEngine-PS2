#include "GenericPlatform/GenericPlatformMath.h"

#include <cmath>

float FGenericPlatformMath::Sin256(uint32 Angle256)
{
	return std::sin(static_cast<float>(Angle256 & 255u) * (6.28318530718f / 256.0f));
}

float FGenericPlatformMath::Cos256(uint32 Angle256)
{
	return std::cos(static_cast<float>(Angle256 & 255u) * (6.28318530718f / 256.0f));
}

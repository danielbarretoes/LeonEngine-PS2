#include "GenericPlatform/GenericPlatformMath.h"

#include <cmath>
#include <cstring>

float FGenericPlatformMath::Sin256(uint32 Angle256)
{
	return std::sin(static_cast<float>(Angle256 & 255u) * (6.28318530718f / 256.0f));
}

float FGenericPlatformMath::Cos256(uint32 Angle256)
{
	return std::cos(static_cast<float>(Angle256 & 255u) * (6.28318530718f / 256.0f));
}

namespace
{
	uint32 GRandSeed = 0;
	uint32 GSRandSeed = 0;
} // namespace

void FGenericPlatformMath::RandInit(int32 Seed)
{
	GRandSeed = uint32(Seed);
}

int32 FGenericPlatformMath::Rand()
{
	// The classic C library LCG; 15 bits like RAND_MAX = 32767.
	GRandSeed = GRandSeed * 1103515245u + 12345u;
	return int32((GRandSeed >> 16) & 0x7fffu);
}

float FGenericPlatformMath::FRand()
{
	return float(Rand()) / 32768.0f;
}

void FGenericPlatformMath::SRandInit(int32 Seed)
{
	GSRandSeed = uint32(Seed);
}

int32 FGenericPlatformMath::GetRandSeed()
{
	return int32(GSRandSeed);
}

float FGenericPlatformMath::SRand()
{
	// UE's SRand: mantissa bits of the seed make a float in [1, 2).
	GSRandSeed = GSRandSeed * 196314165u + 907633515u;
	const uint32 Bits = 0x3F800000u | (GSRandSeed >> 9);
	float Result = 0.0f;
	std::memcpy(&Result, &Bits, sizeof(Result));
	return Result - 1.0f;
}

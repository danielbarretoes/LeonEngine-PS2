#pragma once

#include "Containers/UnrealString.h"
#include "CoreTypes.h"
#include "Math/UnrealMathUtility.h"
#include "Math/Vector.h"

#include <cstring>

/**
 * Deterministic random number generator with its own seed (UE: FRandomStream). The same seed gives the same
 * sequence on every platform, which gameplay code relies on for spread, recoil and bot decisions.
 */
struct CORE_API FRandomStream
{
	/** Seed 0 (UE). */
	FRandomStream()
		: InitialSeed(0)
		, Seed(0)
	{
	}

	explicit FRandomStream(int32 InSeed)
	{
		Initialize(InSeed);
	}

	FORCEINLINE void Initialize(int32 InSeed)
	{
		InitialSeed = InSeed;
		Seed = uint32(InSeed);
	}

	/** Back to the initial seed (UE: Reset). */
	FORCEINLINE void Reset() const
	{
		Seed = uint32(InitialSeed);
	}

	FORCEINLINE int32 GetInitialSeed() const
	{
		return InitialSeed;
	}

	/** Seeds from the global generator (UE: GenerateNewSeed). */
	void GenerateNewSeed()
	{
		Initialize(FMath::Rand());
	}

	/** Float in [0, 1) (UE: GetFraction). */
	FORCEINLINE float GetFraction() const
	{
		MutateSeed();

		const uint32 Bits = 0x3F800000U | (Seed >> 9);
		float Result = 0.0f;
		std::memcpy(&Result, &Bits, sizeof(Result));
		return Result - 1.0f;
	}

	FORCEINLINE uint32 GetUnsignedInt() const
	{
		MutateSeed();
		return Seed;
	}

	/** Uniformly distributed unit vector (UE: GetUnitVector). */
	FVector GetUnitVector() const
	{
		FVector Result;
		float L;

		do
		{
			// Check random vectors in the unit sphere so result is statistically uniform.
			Result.X = GetFraction() * 2.f - 1.f;
			Result.Y = GetFraction() * 2.f - 1.f;
			Result.Z = GetFraction() * 2.f - 1.f;
			L = Result.SizeSquared();
		} while (L > 1.f || L < KINDA_SMALL_NUMBER);

		return Result.GetUnsafeNormal();
	}

	FORCEINLINE int32 GetCurrentSeed() const
	{
		return int32(Seed);
	}

	FORCEINLINE float FRand() const
	{
		return GetFraction();
	}

	/** Integer in [0, A) (UE: RandHelper). */
	FORCEINLINE int32 RandHelper(int32 A) const
	{
		// GetFraction guarantees a result in the [0,1) range.
		return ((A > 0) ? FMath::TruncToInt(GetFraction() * float(A)) : 0);
	}

	/** Integer in [Min, Max] (UE: RandRange). */
	FORCEINLINE int32 RandRange(int32 Min, int32 Max) const
	{
		const int32 Range = (Max - Min) + 1;
		return Min + RandHelper(Range);
	}

	/** Float in [InMin, InMax) (UE: FRandRange). */
	FORCEINLINE float FRandRange(float InMin, float InMax) const
	{
		return InMin + (InMax - InMin) * FRand();
	}

	FORCEINLINE FVector VRand() const
	{
		return GetUnitVector();
	}

	/** Random unit vector in a cone around Dir (UE: VRandCone). */
	FVector VRandCone(const FVector& Dir, float ConeHalfAngleRad) const;

	/** "FRandomStream(InitialSeed=%i, Seed=%u)" (UE). */
	FString ToString() const
	{
		return FString::Printf("FRandomStream(InitialSeed=%i, Seed=%u)", InitialSeed, Seed);
	}

protected:
	/** Linear congruential step (UE: MutateSeed). */
	FORCEINLINE void MutateSeed() const
	{
		Seed = (Seed * 196314165U) + 907633515U;
	}

private:
	int32 InitialSeed;
	mutable uint32 Seed;
};

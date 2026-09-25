#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

	#include "Misc/AutomationTest.h"

/** Golden tables printed by -GoldenRecord (defined in Engine/Private/Tests/LegacyGolden.cpp, test builds only). */
ENGINE_API DECLARE_LOG_CATEGORY_EXTERN(LogGolden, Log, All);

/**
 * Golden results of representative scenarios, recorded in the legacy world (Y up, right-handed, 1 unit = 1 metre)
 * before P7 moves the engine to UE's axes (X forward, Y right, Z up, left-handed, 1 unit = 1 cm).
 *
 * The tables in the Golden tests hold legacy values and never change. Scenes are set up through ToWorld*, results are
 * compared against ToWorld*(table), so a P7 commit only changes these adapters (and the APIs the tests drive).
 *
 * Run a test with -GoldenRecord to print its tables (converted back to legacy with ToLegacy*) instead of checking
 * them: each row is one LogGolden line "<test> | <table> | <values>", ready to paste into Expected<table>[].
 */
namespace LegacyGolden
{
	/** Legacy position (metres, Y up) to the engine world. P7 replaces this with FLegacyCoordinateConversion. */
	inline FVector ToWorldPosition(const FVector& Legacy)
	{
		return Legacy;
	}

	/** Legacy direction (unitless, Y up) to the engine world. P7 replaces this with FLegacyCoordinateConversion. */
	inline FVector ToWorldDirection(const FVector& Legacy)
	{
		return Legacy;
	}

	/** Legacy box half extents (metres, Y up) to the engine world. P7 replaces this with FLegacyCoordinateConversion.
	 */
	inline FVector ToWorldExtent(const FVector& Legacy)
	{
		return Legacy;
	}

	/** Legacy length (metres) to engine units. P7 replaces this with FLegacyCoordinateConversion. */
	inline float ToWorldLength(float Metres)
	{
		return Metres;
	}

	/** Legacy speed (metres per second) to engine units. P7 replaces this with FLegacyCoordinateConversion. */
	inline float ToWorldSpeed(float MetresPerSecond)
	{
		return MetresPerSecond;
	}

	/** Engine-world position back to legacy (for recording). P7 replaces this with FLegacyCoordinateConversion. */
	inline FVector ToLegacyPosition(const FVector& World)
	{
		return World;
	}

	/** Engine-world direction back to legacy (for recording). P7 replaces this with FLegacyCoordinateConversion. */
	inline FVector ToLegacyDirection(const FVector& World)
	{
		return World;
	}

	/** Engine length back to metres (for recording). P7 replaces this with FLegacyCoordinateConversion. */
	inline float ToLegacyLength(float WorldLength)
	{
		return WorldLength;
	}

	/** What a scalar table holds: how it converts between the legacy and the engine world. */
	enum class EUnit : uint8
	{
		/** Ratios, trace times, angles, clip-space coordinates: identical in both worlds. */
		Unitless,
		/** Metres in the table (ToWorldLength). */
		Length,
		/** Metres per second in the table (ToWorldSpeed). */
		Speed,
	};

	/** True when the command line has -GoldenRecord: the Check* helpers print the tables instead of checking. */
	ENGINE_API bool IsRecording();

	/** World positions against legacy positions (ToWorldPosition), per-component tolerance in metres. */
	ENGINE_API bool CheckPositions(FAutomationTestBase& Test, const TCHAR* What, const TArray<FVector>& ActualWorld,
		const FVector* ExpectedLegacy, int32 Count, float TolLegacyMetres);

	/** World directions against legacy directions (ToWorldDirection), unitless per-component tolerance. */
	ENGINE_API bool CheckDirections(FAutomationTestBase& Test, const TCHAR* What, const TArray<FVector>& ActualWorld,
		const FVector* ExpectedLegacy, int32 Count, float Tolerance);

	/** Unitless vectors that do not depend on the world axes (clip-space NDC): compared as they are. */
	ENGINE_API bool CheckUnitlessVectors(FAutomationTestBase& Test, const TCHAR* What, const TArray<FVector>& Actual,
		const FVector* Expected, int32 Count, float Tolerance);

	/** Scalars; Unit says how the legacy table and the tolerance convert to the engine world. */
	ENGINE_API bool CheckScalars(FAutomationTestBase& Test, const TCHAR* What, const TArray<float>& Actual,
		const float* Expected, int32 Count, float Tolerance, EUnit Unit);

	/** Booleans (exact). */
	ENGINE_API bool CheckBools(
		FAutomationTestBase& Test, const TCHAR* What, const TArray<bool>& Actual, const bool* Expected, int32 Count);

	/** Integers such as frame indices and counts (exact). */
	ENGINE_API bool CheckInts(
		FAutomationTestBase& Test, const TCHAR* What, const TArray<int32>& Actual, const int32* Expected, int32 Count);

	/** A hash of a larger result (exact). */
	ENGINE_API bool CheckHash(FAutomationTestBase& Test, const TCHAR* What, uint32 Actual, uint32 Expected);
} // namespace LegacyGolden

#endif // WITH_DEV_AUTOMATION_TESTS

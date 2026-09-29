#include "CoreMinimal.h"
#include "Math/VectorMath.h"
#include "Misc/AutomationTest.h"
#include "Misc/MemStack.h"
#include "Misc/Scratchpad.h"

// The vector math (Docs/PLANS/ps2-shipping.md N15): FVectorMath, the PS2's VU0 in macro mode, against its scalar
// reference FVectorMathFPU. TestPAL runs these on the PS2 (PCSX2) and on Win64, where the two are the same code.

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	/** A deterministic sequence (a linear congruential generator). */
	struct FTestRandom
	{
		uint32 State = 12345;

		float Range(float Min, float Max)
		{
			State = (State * 1664525u) + 1013904223u;
			return Min + ((Max - Min) * (float(State >> 8) / 16777216.0f));
		}
	};

	/** The size of one unit in the last place of a float of X's magnitude (a normal float's). */
	[[nodiscard]] float Ulp(float X)
	{
		uint32 Bits = 0;
		FMemory::Memcpy(&Bits, &X, sizeof(Bits));
		// The exponent's field less the mantissa's 23 bits, as a power of two (no denormals: the EE has none).
		const uint32 Exponent = (Bits >> 23) & 0xffu;
		const uint32 UlpBits = Exponent > 24 ? (Exponent - 23) << 23 : (1u << 23);
		float Result = 0.0f;
		FMemory::Memcpy(&Result, &UlpBits, sizeof(Result));
		return Result;
	}

	/**
	 * The unit of the tolerance of a sum of four products (D2's spirit, N15): one unit in the last place of the sum of
	 * the products' magnitudes, which bounds every partial sum. VU0 accumulates in ACC and truncates, the EE's FPU
	 * rounds each product and each sum on its own: measured in PCSX2, they differ by up to 2 of these units (a sum
	 * whose partial sums are larger than it: 16297.1787 against 16297.1816 from products of 19978.7, -3311.4, 3185.4
	 * and -3555.5), never more; the test allows MaxUnits.
	 */
	[[nodiscard]] float SumTolerance(const float (&Terms)[4])
	{
		const float Scale = FMath::Abs(Terms[0]) + FMath::Abs(Terms[1]) + FMath::Abs(Terms[2]) + FMath::Abs(Terms[3]);
		return Ulp(FMath::Max(Scale, 1.0e-30f));
	}

	FMatrix RandomMatrix(FTestRandom& Random, float Extent)
	{
		FMatrix Result;
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Col = 0; Col < 4; ++Col)
			{
				Result.M[Row][Col] = Random.Range(-Extent, Extent);
			}
		}
		return Result;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVectorMathVU0Test, "System.Core.Math.VectorMathVU0",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FVectorMathVU0Test::RunTest(const FString& Parameters)
{
	// Matrix x matrix, matrix x vector and the frustum's box and sphere tests on the platform's vector unit and on the
	// scalar reference: every sum within MaxUnits of SumTolerance (units in the last place of its products'
	// magnitudes); a box or a sphere on the same side of every plane unless it touches one within that tolerance.
	FTestRandom Random;
	constexpr float MaxUnits = 2.0f;
	float WorstUnits = 0.0f;
	float WorstTransformUnits = 0.0f;
	int32 NumOutside = 0;
	for (int32 Pass = 0; Pass < 200; ++Pass)
	{
		// Transforms as the engine makes them (rotations, scales and moves of centimetres) and plain random ones.
		const FMatrix A = Pass % 2 == 0
			? FMatrix(FScaleMatrix(FVector(Random.Range(0.5f, 2.0f))) *
				  FQuatRotationMatrix(
					  FQuat(FVector(Random.Range(-1.0f, 1.0f), Random.Range(-1.0f, 1.0f), 1.0f).GetSafeNormal(),
						  Random.Range(-3.0f, 3.0f))))
			: RandomMatrix(Random, 100.0f);
		FMatrix B = RandomMatrix(Random, Pass % 3 == 0 ? 1.0f : 1000.0f);
		FMatrix Vector;
		FMatrix Reference;
		FVectorMath::MatrixMultiply(Vector, A, B);
		FVectorMathFPU::MatrixMultiply(Reference, A, B);
		for (int32 Row = 0; Row < 4; ++Row)
		{
			for (int32 Col = 0; Col < 4; ++Col)
			{
				const float Terms[4] = {A.M[Row][0] * B.M[0][Col], A.M[Row][1] * B.M[1][Col], A.M[Row][2] * B.M[2][Col],
					A.M[Row][3] * B.M[3][Col]};
				const float Units = FMath::Abs(Vector.M[Row][Col] - Reference.M[Row][Col]) / SumTolerance(Terms);
				WorstUnits = FMath::Max(WorstUnits, Units);
			}
		}
		// In place, as FMatrix::operator*= does.
		FMatrix InPlace = A;
		FVectorMath::MatrixMultiply(InPlace, InPlace, B);
		TestTrue("A product in place", InPlace.Equals(Vector, 0.0f));

		const FVector4 P(Random.Range(-500.0f, 500.0f), Random.Range(-500.0f, 500.0f), Random.Range(-500.0f, 500.0f),
			Pass % 4 == 0 ? 0.0f : 1.0f);
		FVector4 Transformed;
		FVector4 TransformedReference;
		FVectorMath::TransformVector4(Transformed, B, P);
		FVectorMathFPU::TransformVector4(TransformedReference, B, P);
		for (int32 Col = 0; Col < 4; ++Col)
		{
			const float Terms[4] = {P.X * B.M[0][Col], P.Y * B.M[1][Col], P.Z * B.M[2][Col], P.W * B.M[3][Col]};
			const float Units = FMath::Abs(Transformed[Col] - TransformedReference[Col]) / SumTolerance(Terms);
			WorstTransformUnits = FMath::Max(WorstTransformUnits, Units);
		}

		// Six unit planes around the origin and a box and a sphere somewhere near them.
		FVectorPlaneSet Planes;
		for (int32 Plane = 0; Plane < 6; ++Plane)
		{
			const FVector Normal =
				FVector(Random.Range(-1.0f, 1.0f), Random.Range(-1.0f, 1.0f), Random.Range(-1.0f, 1.0f))
					.GetSafeNormal();
			Planes.AddPlane(Normal.X, Normal.Y, Normal.Z, Random.Range(0.0f, 800.0f));
		}
		const FVector Center(
			Random.Range(-1500.0f, 1500.0f), Random.Range(-1500.0f, 1500.0f), Random.Range(-1500.0f, 1500.0f));
		const FVector Extent(Random.Range(1.0f, 200.0f), Random.Range(1.0f, 200.0f), Random.Range(1.0f, 200.0f));
		const float Radius = Extent.Size();
		const bool bBoxOutside = FVectorMath::IsBoxOutside(Planes, Center, Extent);
		const bool bSphereOutside = FVectorMath::IsSphereOutside(Planes, Center, Radius);
		NumOutside += bBoxOutside ? 1 : 0;
		// Only a shape within a tolerance of a plane may land on the other side.
		float Nearest = TNumericLimits<float>::Max();
		for (int32 Plane = 0; Plane < Planes.NumPlanes; ++Plane)
		{
			const float Distance = (Planes.X[Plane] * Center.X) + (Planes.Y[Plane] * Center.Y) +
				(Planes.Z[Plane] * Center.Z) + Planes.W[Plane];
			const float Reach =
				(Planes.AbsX[Plane] * Extent.X) + (Planes.AbsY[Plane] * Extent.Y) + (Planes.AbsZ[Plane] * Extent.Z);
			Nearest = FMath::Min(Nearest, FMath::Min(FMath::Abs(Distance + Reach), FMath::Abs(Distance + Radius)));
		}
		if (Nearest > 0.01f)
		{
			TestEqual(
				"The box on the reference's side", bBoxOutside, FVectorMathFPU::IsBoxOutside(Planes, Center, Extent));
			TestEqual("The sphere on the reference's side", bSphereOutside,
				FVectorMathFPU::IsSphereOutside(Planes, Center, Radius));
		}
	}
	UE_LOG(LogTemp, Display, "%s",
		*FString::Printf("Vector math (%s): the largest differences from the scalar reference are %.3f (matrix x "
						 "matrix) and %.3f (matrix x vector) of the tolerance (one unit in the last place of the "
						 "products' magnitudes); %d of 200 boxes outside",
			FVectorMath::IsVectorUnit() ? "VU0" : "the reference itself", double(WorstUnits),
			double(WorstTransformUnits), NumOutside));
	TestTrue("Within the tolerance", WorstUnits <= MaxUnits && WorstTransformUnits <= MaxUnits);
	TestTrue("Some boxes in, some out", NumOutside > 0 && NumOutside < 200);

	// Known values: a unit box 10 cm in front of a plane facing it, and 10 cm behind it.
	FVectorPlaneSet Floor;
	Floor.AddPlane(0.0f, 0.0f, 1.0f, 0.0f);
	TestFalse("Above the floor", FVectorMath::IsBoxOutside(Floor, FVector(0.0f, 0.0f, 10.0f), FVector(1.0f)));
	TestTrue("Under the floor", FVectorMath::IsBoxOutside(Floor, FVector(0.0f, 0.0f, -10.0f), FVector(1.0f)));
	TestFalse("Across the floor", FVectorMath::IsSphereOutside(Floor, FVector(0.0f, 0.0f, -10.0f), 11.0f));
	TestTrue("Every unused slot inside", !FVectorMath::IsBoxOutside(FVectorPlaneSet(), FVector(1.0e6f), FVector(1.0f)));
	const FMatrix Identity = FMatrix::Identity * FMatrix::Identity;
	TestTrue("Identity times identity", Identity.Equals(FMatrix::Identity, 0.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FScratchpadTest, "System.Core.Memory.Scratchpad",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FScratchpadTest::RunTest(const FString& Parameters)
{
	// The scratchpad (N15): pushes inside a mark come from its 16 KB and go when the mark does; the array at its top
	// grows in place; what does not fit comes from the frame's stack (counted), and a container of either kind frees
	// nothing by itself.
	FScratchpad& Scratchpad = FScratchpad::Get();
	FMemMark StackMark(FMemStack::Get());
	const SIZE_T UsedBefore = Scratchpad.GetUsedBytes();
	const int32 OverflowsBefore = Scratchpad.GetNumOverflows();
	{
		FScratchpadMark Mark;
		TArray<int32, TScratchpadAllocator<>> Numbers;
		Numbers.Reserve(8);
		const int32* First = Numbers.GetData();
		TestTrue("On the scratchpad", Scratchpad.Contains(First));
		for (int32 Index = 0; Index < 100; ++Index)
		{
			Numbers.Add(Index);
		}
		TestTrue("Grown in place at the top", Numbers.GetData() == First);
		TestEqual("Kept its values", Numbers[99], 99);
		TArray<uint8, TScratchpadAllocator<>> Big;
		Big.SetNumUninitialized(int32(FScratchpad::Size));
		TestFalse("Too big: on the frame's stack", Scratchpad.Contains(Big.GetData()));
		TestEqual("One overflow", Scratchpad.GetNumOverflows(), OverflowsBefore + 1);
		Numbers.Add(100);
		TestTrue("Still where it was (the big one is not on the scratchpad)", Numbers.GetData() == First);
		TestTrue("In use", Scratchpad.GetUsedBytes() >= UsedBefore + (101 * sizeof(int32)));
	}
	TestEqual("Given back with the mark", Scratchpad.GetUsedBytes(), UsedBefore);
	TestTrue("At most its size", Scratchpad.GetPeakBytes() <= FScratchpad::Size);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

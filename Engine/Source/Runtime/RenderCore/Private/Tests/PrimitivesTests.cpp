#include "CoreMinimal.h"
#include "LegacyCoordinateConversion.h"
#include "Misc/AutomationTest.h"
#include "Primitives.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** The legacy (Y-up) cube, in cm: the table the engine had before the Z-up switch. */
	FMeshData MakeLegacyCube()
	{
		constexpr float H = 50.0f;
		FMeshData Data;
		Data.Vertices = {
			FVertex(FVector(-H, -H, H), FVector(0, 0, 1), FVector2D(0, 0)),
			FVertex(FVector(H, -H, H), FVector(0, 0, 1), FVector2D(1, 0)),
			FVertex(FVector(H, H, H), FVector(0, 0, 1), FVector2D(1, 1)),
			FVertex(FVector(-H, H, H), FVector(0, 0, 1), FVector2D(0, 1)),
			FVertex(FVector(H, -H, -H), FVector(0, 0, -1), FVector2D(0, 0)),
			FVertex(FVector(-H, -H, -H), FVector(0, 0, -1), FVector2D(1, 0)),
			FVertex(FVector(-H, H, -H), FVector(0, 0, -1), FVector2D(1, 1)),
			FVertex(FVector(H, H, -H), FVector(0, 0, -1), FVector2D(0, 1)),
			FVertex(FVector(-H, H, H), FVector(0, 1, 0), FVector2D(0, 0)),
			FVertex(FVector(H, H, H), FVector(0, 1, 0), FVector2D(1, 0)),
			FVertex(FVector(H, H, -H), FVector(0, 1, 0), FVector2D(1, 1)),
			FVertex(FVector(-H, H, -H), FVector(0, 1, 0), FVector2D(0, 1)),
			FVertex(FVector(-H, -H, -H), FVector(0, -1, 0), FVector2D(0, 0)),
			FVertex(FVector(H, -H, -H), FVector(0, -1, 0), FVector2D(1, 0)),
			FVertex(FVector(H, -H, H), FVector(0, -1, 0), FVector2D(1, 1)),
			FVertex(FVector(-H, -H, H), FVector(0, -1, 0), FVector2D(0, 1)),
			FVertex(FVector(H, -H, H), FVector(1, 0, 0), FVector2D(0, 0)),
			FVertex(FVector(H, -H, -H), FVector(1, 0, 0), FVector2D(1, 0)),
			FVertex(FVector(H, H, -H), FVector(1, 0, 0), FVector2D(1, 1)),
			FVertex(FVector(H, H, H), FVector(1, 0, 0), FVector2D(0, 1)),
			FVertex(FVector(-H, -H, -H), FVector(-1, 0, 0), FVector2D(0, 0)),
			FVertex(FVector(-H, -H, H), FVector(-1, 0, 0), FVector2D(1, 0)),
			FVertex(FVector(-H, H, H), FVector(-1, 0, 0), FVector2D(1, 1)),
			FVertex(FVector(-H, H, -H), FVector(-1, 0, 0), FVector2D(0, 1)),
		};
		for (uint32 Face = 0; Face < 6; ++Face)
		{
			const uint32 B = Face * 4;
			Data.Indices.Append({B + 0, B + 1, B + 2, B + 0, B + 2, B + 3});
		}
		return Data;
	}

	/** The legacy plane on XZ (Y = 0). */
	FMeshData MakeLegacyPlane(float Size)
	{
		const float H = Size * 0.5f;
		FMeshData Data;
		Data.Vertices = {
			FVertex(FVector(-H, 0.0f, -H), FVector(0.0f, 1.0f, 0.0f), FVector2D(0.0f, 0.0f)),
			FVertex(FVector(H, 0.0f, -H), FVector(0.0f, 1.0f, 0.0f), FVector2D(1.0f, 0.0f)),
			FVertex(FVector(H, 0.0f, H), FVector(0.0f, 1.0f, 0.0f), FVector2D(1.0f, 1.0f)),
			FVertex(FVector(-H, 0.0f, H), FVector(0.0f, 1.0f, 0.0f), FVector2D(0.0f, 1.0f)),
		};
		Data.Indices = {0, 2, 1, 0, 3, 2};
		return Data;
	}

	/** The legacy UV sphere, poles on Y. */
	FMeshData MakeLegacySphere(int32 Segments, int32 Rings)
	{
		FMeshData Data;
		for (int32 Ring = 0; Ring <= Rings; ++Ring)
		{
			const float V = static_cast<float>(Ring) / static_cast<float>(Rings);
			const float Phi = V * PI;
			for (int32 Segment = 0; Segment <= Segments; ++Segment)
			{
				const float U = static_cast<float>(Segment) / static_cast<float>(Segments);
				const float Theta = U * 2.0f * PI;
				const FVector Normal(
					FMath::Cos(Theta) * FMath::Sin(Phi), FMath::Cos(Phi), FMath::Sin(Theta) * FMath::Sin(Phi));
				Data.Vertices.Add(FVertex(Normal * 50.0f, Normal, FVector2D(U, 1.0f - V)));
			}
		}
		for (int32 Ring = 0; Ring < Rings; ++Ring)
		{
			for (int32 Segment = 0; Segment < Segments; ++Segment)
			{
				const uint32 I0 = static_cast<uint32>(Ring * (Segments + 1) + Segment);
				const uint32 I1 = I0 + static_cast<uint32>(Segments + 1);
				Data.Indices.Append({I0, I0 + 1, I1, I0 + 1, I1 + 1, I1});
			}
		}
		return Data;
	}

	/** Checks that Engine is the legacy mesh in the engine basis (the positions are already in cm). */
	void CheckConvertedLegacy(FAutomationTestBase& Test, const TCHAR* What, const FMeshData& Engine, FMeshData Legacy)
	{
		FLegacyCoordinateConversion::ConvertMeshData(Legacy);
		if (!Test.TestEqual(*FString::Printf("%s vertex count", What), Engine.Vertices.Num(), Legacy.Vertices.Num()) ||
			!Test.TestTrue(*FString::Printf("%s indices", What), Engine.Indices == Legacy.Indices))
		{
			return;
		}
		for (int32 Index = 0; Index < Engine.Vertices.Num(); ++Index)
		{
			const FVertex& A = Engine.Vertices[Index];
			const FVertex& B = Legacy.Vertices[Index];
			const FVector LegacyPositionCm = B.Position / FLegacyCoordinateConversion::UnitsPerMetre;
			if (!A.Position.Equals(LegacyPositionCm, 1.0e-3f) || !A.Normal.Equals(B.Normal, 1.0e-5f) ||
				A.TexCoord.X != B.TexCoord.X || A.TexCoord.Y != B.TexCoord.Y)
			{
				Test.AddError(FString::Printf("%s vertex %d differs from the converted legacy vertex", What, Index));
				return;
			}
		}
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMakeCubeTest, "System.RenderCore.Primitives.MakeCube",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMakeCubeTest::RunTest(const FString& Parameters)
{
	const FMeshData Cube = MakeCube();
	TestFalse("Not empty", Cube.IsEmpty());
	TestEqual("Vertices", Cube.Vertices.Num(), 24);
	TestEqual("Indices", Cube.Indices.Num(), 36);
	for (const FVertex& V : Cube.Vertices)
	{
		TestTrue("Inside the 100 cm box",
			FMath::Abs(V.Position.X) <= 50.0f + 1.0e-3f && FMath::Abs(V.Position.Y) <= 50.0f + 1.0e-3f &&
				FMath::Abs(V.Position.Z) <= 50.0f + 1.0e-3f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMakePlaneTest, "System.RenderCore.Primitives.MakePlane",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMakePlaneTest::RunTest(const FString& Parameters)
{
	// The plane lies on XY and faces +Z.
	const FMeshData Plane = MakePlane(200.0f);
	TestFalse("Not empty", Plane.IsEmpty());
	for (const FVertex& V : Plane.Vertices)
	{
		TestEqual("Z", V.Position.Z, 0.0f, 1.0e-5f);
		TestTrue("Normal +Z", V.Normal.Equals(FVector(0.0f, 0.0f, 1.0f)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMakeSphereTest, "System.RenderCore.Primitives.MakeSphere",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FMakeSphereTest::RunTest(const FString& Parameters)
{
	// Too-low tessellation is clamped to a valid mesh; the first ring is the top pole (+Z).
	const FMeshData Sphere = MakeSphere(2, 1);
	TestFalse("Not empty", Sphere.IsEmpty());
	TestEqual("Triangles", Sphere.Indices.Num() % 3, 0);
	TestTrue("Top pole", Sphere.Vertices[0].Position.Equals(FVector(0.0f, 0.0f, 50.0f), 1.0e-3f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPrimitivesAreConvertedLegacyPrimitivesTest,
	"System.RenderCore.Primitives.AreConvertedLegacyPrimitives",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FPrimitivesAreConvertedLegacyPrimitivesTest::RunTest(const FString& Parameters)
{
	// The basic shapes are the legacy Y-up shapes converted to the engine basis: the same triangles in the same index
	// order (so the same winding on screen), with Y and Z swapped.
	CheckConvertedLegacy(*this, "Cube", MakeCube(), MakeLegacyCube());
	CheckConvertedLegacy(*this, "Plane", MakePlane(300.0f), MakeLegacyPlane(300.0f));
	CheckConvertedLegacy(*this, "Sphere", MakeSphere(12, 8), MakeLegacySphere(12, 8));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

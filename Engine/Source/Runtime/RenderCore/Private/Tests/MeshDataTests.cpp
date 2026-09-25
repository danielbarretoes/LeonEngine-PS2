#include "CoreMinimal.h"
#include "LegacyCoordinateConversion.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "Primitives.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComputeTangentsTest, "System.RenderCore.MeshData.ComputeTangents",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FComputeTangentsTest::RunTest(const FString& Parameters)
{
	// Recomputed from the cube's UVs, every tangent is a unit vector with a +-1 handedness.
	FMeshData Data = MakeCube();
	for (FVertex& V : Data.Vertices)
	{
		V.Tangent = FVector4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	ComputeTangents(Data);
	for (const FVertex& V : Data.Vertices)
	{
		TestEqual("Unit tangent", FVector(V.Tangent).Size(), 1.0f, 1.0e-3f);
		TestTrue("Handedness", V.Tangent.W == 1.0f || V.Tangent.W == -1.0f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FComputeTangentsFallbackMatchesLegacyTest,
	"System.RenderCore.MeshData.ComputeTangentsFallbackMatchesLegacy",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FComputeTangentsFallbackMatchesLegacyTest::RunTest(const FString& Parameters)
{
	// Without UVs every vertex takes the fallback tangent. Computed in the engine basis it is the legacy fallback
	// converted (Y and Z swapped, the bitangent sign flipped), for a tilted and for a vertical normal.
	const FVector Normals[2] = {FVector(0.6f, 0.0f, 0.8f), FVector(0.0f, 1.0f, 0.0f)};
	for (const FVector& LegacyNormal : Normals)
	{
		FMeshData Legacy;
		for (int32 Corner = 0; Corner < 3; ++Corner)
		{
			Legacy.Vertices.Add(
				FVertex(FVector(static_cast<float>(Corner), 0.0f, 0.0f), LegacyNormal, FVector2D(0.0f, 0.0f)));
		}
		Legacy.Indices = {0, 1, 2};
		FMeshData Engine = Legacy;
		FLegacyCoordinateConversion::ConvertMeshData(Engine);

		ComputeTangents(Legacy, EMeshDataBasis::LegacyYUp);
		ComputeTangents(Engine);
		const FVector4 Expected = FLegacyCoordinateConversion::ConvertTangent(Legacy.Vertices[0].Tangent);
		const FVector4 Actual = Engine.Vertices[0].Tangent;
		TestTrue(*FString::Printf("Tangent for normal (%g, %g, %g)", static_cast<double>(LegacyNormal.X),
					 static_cast<double>(LegacyNormal.Y), static_cast<double>(LegacyNormal.Z)),
			FVector(Actual).Equals(FVector(Expected), 1.0e-6f) && Actual.W == Expected.W);
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "LegacyCoordinateConversion.h"
#include "LeonMeshFormat.h"
#include "MeshData.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/** Rewrites the version field (after the magic) of a .lmesh. */
	[[nodiscard]] bool SetLeonMeshVersion(const FString& Path, uint32 Version)
	{
		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *Path) || Bytes.Num() < 8)
		{
			return false;
		}
		FMemory::Memcpy(Bytes.GetData() + 4, &Version, sizeof(uint32));
		return FFileHelper::SaveArrayToFile(Bytes, *Path);
	}

	[[nodiscard]] bool SameVertices(const FMeshData& A, const FMeshData& B)
	{
		return A.Vertices.Num() == B.Vertices.Num() &&
			FMemory::Memcmp(A.Vertices.GetData(), B.Vertices.GetData(),
				sizeof(FVertex) * static_cast<SIZE_T>(A.Vertices.Num())) == 0;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonMeshFormatVersionsTest, "System.RenderCore.LeonMeshFormat.Versions",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonMeshFormatVersionsTest::RunTest(const FString& Parameters)
{
	// The writer emits version 2 (engine world data, loaded as stored); the same bytes marked version 1 are legacy data
	// and load converted; any other version is rejected.
	FMeshData Data;
	Data.Vertices.Add(FVertex(FVector(1.0f, 2.0f, 3.0f), FVector(0.0f, 0.6f, 0.8f), FVector2D(0.25f, 0.5f),
		FVector4(1.0f, 0.0f, 0.0f, -1.0f)));
	Data.Vertices.Add(FVertex(FVector(-4.0f, 5.0f, -6.0f), FVector(0.0f, 0.0f, 1.0f), FVector2D(0.75f, 1.0f),
		FVector4(0.0f, 1.0f, 0.0f, 1.0f)));
	Data.Vertices.Add(FVertex(FVector(7.0f, -8.0f, 9.0f), FVector(1.0f, 0.0f, 0.0f), FVector2D(0.0f, 0.125f),
		FVector4(0.0f, 0.0f, 1.0f, 1.0f)));
	Data.Indices = {0, 2, 1};
	Data.Submeshes.Add(FMeshSection{0, 3, 0});
	Data.Materials.AddDefaulted();
	Data.AlbedoMapPaths.AddDefaulted();

	const FString Path = FPaths::CreateTempFilename(*FPaths::EngineIntermediateDir(), "leon_test_mesh", ".lmesh");
	if (!TestTrue("Saved", SaveLeonMeshFile(Path, Data)))
	{
		return false;
	}
	TArray<uint8> Bytes;
	uint32 Version = 0;
	if (TestTrue("Read back", FFileHelper::LoadFileToArray(Bytes, *Path) && Bytes.Num() >= 8))
	{
		FMemory::Memcpy(&Version, Bytes.GetData() + 4, sizeof(uint32));
	}
	TestEqual("Written version", Version, 2u);

	FMeshData Loaded;
	TestTrue("Version 2 loads", LoadLeonMeshFile(Path, Loaded));
	TestTrue("Version 2 loads as stored", SameVertices(Loaded, Data) && Loaded.Indices == Data.Indices);

	FMeshData Legacy;
	FMeshData Expected = Data;
	FLegacyCoordinateConversion::ConvertMeshData(Expected);
	TestTrue("Marked version 1", SetLeonMeshVersion(Path, 1));
	TestTrue("Version 1 loads", LoadLeonMeshFile(Path, Legacy));
	TestTrue("Version 1 loads converted", SameVertices(Legacy, Expected) && Legacy.Indices == Data.Indices);

	FMeshData Rejected;
	TestTrue("Marked version 3", SetLeonMeshVersion(Path, 3));
	AddExpectedError("Bad header", 1);
	TestFalse("Version 3 is rejected", LoadLeonMeshFile(Path, Rejected));

	IFileManager::Get().Delete(*Path);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

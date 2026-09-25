#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "HAL/PlatformFilemanager.h"
#include "IPlatformFilePak.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PakWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

// The test targets link PakFile (LeonAutomationTests): a cooked game loads its packages from its pak through the
// platform file chain, which this test runs end to end with an engine asset.

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPakFileLoadsAssetTest, "System.Engine.PakFile.LoadsAssetFromPak",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FPakFileLoadsAssetTest::RunTest(const FString& Parameters)
{
	// An asset package that exists only inside a pak: with the pak platform file on top of the chain,
	// FPackageName::DoesPackageExist finds it and LoadPackage loads the static mesh and its imports (the engine's
	// default material, from the loose engine content below the pak).
	TArray<uint8> CubeBytes;
	const FString CubeFile = FPaths::EngineContentDir() + TEXT("BasicShapes/Cube.lasset");
	if (!FFileHelper::LoadFileToArray(CubeBytes, *CubeFile))
	{
		AddError(TEXT("Engine/Content/BasicShapes/Cube.lasset not found"));
		return false;
	}
	const FString Root = FPaths::ProjectIntermediateDir() + TEXT("Tests/EnginePak/");
	const FString PackageName = TEXT("/EnginePakTest/Shapes/Cube");
	FPackageName::RegisterMountPoint(TEXT("/EnginePakTest/"), Root);

	FPakWriter Writer;
	Writer.AddFile(TEXT("../../../Shapes/Cube.lasset"), MoveTemp(CubeBytes));
	TArray<uint8> Pak;
	TestTrue(TEXT("Pak written"), Writer.Finalize(Pak));
	FPakPlatformFile PakPlatformFile;
	PakPlatformFile.SetLowerLevel(&FPlatformFileManager::Get().GetPlatformFile());
	TestTrue(TEXT("Mounted"),
		PakPlatformFile.MountFromMemory(TEXT("EngineTest.lpak"), MoveTemp(Pak), 0, *(Root + TEXT("Shapes/"))));
	TestFalse(TEXT("Not on disk"), FPackageName::DoesPackageExist(PackageName));

	IPlatformFile& Previous = FPlatformFileManager::Get().GetPlatformFile();
	FPlatformFileManager::Get().SetPlatformFile(PakPlatformFile);
	FString Filename;
	TestTrue(TEXT("In the pak"), FPackageName::DoesPackageExist(PackageName, nullptr, &Filename));
	UPackage* Package = LoadPackage(nullptr, *PackageName, LOAD_None);
	FPlatformFileManager::Get().SetPlatformFile(Previous);
	if (TestNotNull(TEXT("Loaded from the pak"), Package))
	{
		const UStaticMesh* Mesh = FindObject<UStaticMesh>(Package, TEXT("Cube"));
		if (TestNotNull(TEXT("The static mesh"), Mesh))
		{
			TestTrue(TEXT("Its vertices"), Mesh->GetLODResources().Vertices.Num() > 0);
		}
		TestTrue(TEXT("The file is the pak's"), FPaths::IsUnderDirectory(Package->FileName.ToString(), Root));
		TArray<UObject*> Objects;
		GetObjectsWithOuter(Package, Objects, true);
		for (UObject* Object : Objects)
		{
			Object->MarkPendingKill();
		}
		Package->MarkPendingKill();
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, true);
	}
	FPackageName::UnRegisterMountPoint(TEXT("/EnginePakTest/"), Root);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

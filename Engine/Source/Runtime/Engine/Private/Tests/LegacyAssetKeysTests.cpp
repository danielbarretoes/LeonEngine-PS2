#include "CoreMinimal.h"
#include "HAL/FileManager.h"
#include "Level/LegacyAssetKeys.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

// The content keys of the legacy `.llev` / `.lmat` files and the packages they name after the migration (P14 part 2).

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyAssetKeysNamesTest, "System.Engine.LegacyAssetKeys.MigratedNames",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyAssetKeysNamesTest::RunTest(const FString& Parameters)
{
	// A key loses its extension, gains the prefix of its class, and the engine's legacy folders are UE's.
	TestEqual("lmat", FLegacyAssetKeys::GetPrefixForExtension(TEXT(".lmat")), FString("M_"));
	TestEqual("lmesh", FLegacyAssetKeys::GetPrefixForExtension(TEXT("lmesh")), FString("SM_"));
	TestEqual("PNG", FLegacyAssetKeys::GetPrefixForExtension(TEXT("PNG")), FString("T_"));
	TestEqual("wav", FLegacyAssetKeys::GetPrefixForExtension(TEXT("wav")), FString("S_"));
	TestEqual("other", FLegacyAssetKeys::GetPrefixForExtension(TEXT("llev")), FString());
	TestEqual(
		"Normalized", FLegacyAssetKeys::NormalizeKey(TEXT("assets/materials\\M_X.lmat")), FString("materials/M_X"));
	TestEqual("The Starter's material",
		FLegacyAssetKeys::GetMigratedPackageName(TEXT("/Engine"), TEXT("materials/M_WorldGrid.lmat")),
		FString("/Engine/EngineMaterials/M_WorldGrid"));
	TestEqual("An engine texture",
		FLegacyAssetKeys::GetMigratedPackageName(TEXT("/Engine"), TEXT("Textures/T_Default_D.png")),
		FString("/Engine/EngineMaterials/T_Default_D"));
	TestEqual("Prefixed", FLegacyAssetKeys::GetMigratedPackageName(TEXT("/Game"), TEXT("Materials/Red.lmat")),
		FString("/Game/Materials/M_Red"));
	TestEqual("A mesh", FLegacyAssetKeys::GetMigratedPackageName(TEXT("/Game"), TEXT("Meshes/Crate.lmesh")),
		FString("/Game/Meshes/SM_Crate"));
	TestEqual("Only the engine's folders move",
		FLegacyAssetKeys::GetMigratedPackageName(TEXT("/Game"), TEXT("Textures/T_A.png")),
		FString("/Game/Textures/T_A"));

	TArray<FString> Candidates;
	FLegacyAssetKeys::GetCandidatePackageNames(TEXT("/Game"), TEXT("Materials/Red.lmat"), Candidates);
	TestTrue("Candidates: the content root, /Game, /Engine; migrated first",
		Candidates.Num() >= 4 && Candidates[0] == TEXT("/Game/Materials/M_Red") &&
			Candidates[1] == TEXT("/Game/Materials/Red") && Candidates[2] == TEXT("/Engine/EngineMaterials/M_Red"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLegacyAssetKeysResolveTest, "System.Engine.LegacyAssetKeys.ResolveAndMount",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLegacyAssetKeysResolveTest::RunTest(const FString& Parameters)
{
	// A content folder under a mount point uses its path; any other gets a mount point named after it (a suffix when
	// the name is taken); a key resolves to the first candidate package that exists.
	TestEqual("The engine's content", FLegacyAssetKeys::MountContentDirectory(FPaths::EngineContentDir()),
		FString("/Engine"));
	const FString DirA = FPaths::ProjectIntermediateDir() + TEXT("Tests/LegacyAssetKeys/A/KeyPack");
	const FString DirB = FPaths::ProjectIntermediateDir() + TEXT("Tests/LegacyAssetKeys/B/KeyPack");
	IFileManager::Get().MakeDirectory(*DirA, true);
	IFileManager::Get().MakeDirectory(*DirB, true);
	const FString RootA = FLegacyAssetKeys::MountContentDirectory(DirA);
	const FString RootB = FLegacyAssetKeys::MountContentDirectory(DirB);
	TestEqual("Named after the folder", RootA, FString("/KeyPack"));
	TestEqual("Taken: a suffix", RootB, FString("/KeyPack_2"));
	TestEqual("Mounted once", FLegacyAssetKeys::MountContentDirectory(DirA), RootA);

	UMaterial* Red = NewObject<UMaterial>(CreatePackage(TEXT("/KeyPack/Materials/M_Red")), TEXT("M_Red"), RF_Public);
	TestEqual("Resolved", FLegacyAssetKeys::ResolveKey(RootA, TEXT("Materials/Red.lmat")),
		FString("/KeyPack/Materials/M_Red.M_Red"));
	TestEqual("Not under the other root", FLegacyAssetKeys::ResolveKey(RootB, TEXT("Materials/Red.lmat")), FString());
	TestEqual("Nothing there", FLegacyAssetKeys::ResolveKey(RootA, TEXT("Materials/Blue.lmat")), FString());

	Red->MarkPendingKill();
	Red->GetOutermost()->MarkPendingKill();
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	FPackageName::UnRegisterMountPoint(RootA + TEXT("/"), DirA);
	FPackageName::UnRegisterMountPoint(RootB + TEXT("/"), DirB);
	IFileManager::Get().DeleteDirectory(
		*(FPaths::ProjectIntermediateDir() + TEXT("Tests/LegacyAssetKeys")), false, true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

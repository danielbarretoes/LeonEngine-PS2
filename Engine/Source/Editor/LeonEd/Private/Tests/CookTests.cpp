#include "AssetImportUtils.h"
#include "Commandlets/CookCommandlet.h"
#include "Commandlets/ImportAssetsCommandlet.h"
#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Interfaces/ITargetPlatform.h"
#include "Interfaces/ITargetPlatformManagerModule.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Templates/UniquePtr.h"
#include "Tests/CookTestTypes.h"
#include "Tests/LeonEdTestUtils.h"
#include "UObject/LinkerLoad.h"

#if WITH_DEV_AUTOMATION_TESTS

// The cook (UCookCommandlet): its seeds from the config, the dependency closure over the packages' tables, the cooked
// packages (no editor-only data, the target platform recorded, the same bytes every time) and the staged config and
// shaders.

namespace
{
	const TArray<uint8> CookRGB = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120};

	/** Imports a 2x2 texture into the test mount point (with its editor-only import data). */
	UTexture2D* ImportTexture(const TCHAR* Name)
	{
		const FString Source =
			LeonEdTest::WriteSource(FString(Name) + TEXT(".bmp"), LeonEdTest::MakeBmp(2, 2, CookRGB));
		return Cast<UTexture2D>(
			UImportAssetsCommandlet::ImportAsset(Source, TEXT("/LeonEdTest"), FString(), FString(), {}));
	}

	/** A saved asset with a hard and a soft reference. */
	bool SaveCookTestAsset(const TCHAR* PackageName, UObject* HardRef, const FSoftObjectPath& SoftRef)
	{
		UPackage* Package = CreatePackage(PackageName);
		UCookTestAsset* Asset = NewObject<UCookTestAsset>(
			Package, *FPackageName::GetShortName(FString(PackageName)), RF_Public | RF_Standalone);
		Asset->HardRef = HardRef;
		Asset->SoftRef = TSoftObjectPtr<UObject>(SoftRef);
		return FAssetImportUtils::SavePackage(Package, Asset);
	}

	/** Whether a package's tables export an object of the class ClassName. */
	bool ExportsClass(const FLinkerLoad& Tables, const TCHAR* ClassName)
	{
		for (int32 Index = 0; Index < Tables.ExportMap.Num(); ++Index)
		{
			if (const_cast<FLinkerLoad&>(Tables).GetExportClassName(Index) == FName(ClassName))
			{
				return true;
			}
		}
		return false;
	}

	ITargetPlatform* FindPlatform(const TCHAR* Name)
	{
		ITargetPlatformManagerModule* Manager = GetTargetPlatformManager();
		return Manager != nullptr ? Manager->FindTargetPlatform(Name) : nullptr;
	}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdCookTargetPlatformsTest, "System.LeonEd.Cook.TargetPlatforms",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdCookTargetPlatformsTest::RunTest(const FString& Parameters)
{
	// Win64 is the identity target, PS2 a stub that says what it does not convert yet; neither keeps editor-only data;
	// the default config seeds the cook with the maps the game opens, the engine's default assets and the folders
	// cooked whole, and not with a map nothing asks for (AxisTest).
	ITargetPlatform* Win64 = FindPlatform(TEXT("win64"));
	ITargetPlatform* PS2 = FindPlatform(TEXT("PS2"));
	if (!TestNotNull("Win64 (case-insensitive)", Win64) || !TestNotNull("PS2", PS2))
	{
		return false;
	}
	TestNull("An unknown platform", FindPlatform(TEXT("Switch")));
	#if PLATFORM_WINDOWS
	// The development platform is Win64 (a Linux build runs the tests too, but Linux is no target platform).
	TestTrue("The running platform", GetTargetPlatformManagerRef().GetRunningTargetPlatform() == Win64);
	#endif
	TestEqual("Win64 config platform", Win64->IniPlatformName(), FString(TEXT("Windows")));
	TestEqual("PS2 config platform", PS2->IniPlatformName(), FString(TEXT("PS2")));
	TestEqual("Win64 records its name", Win64->CookedPlatformName(), FString(TEXT("Win64")));
	TestFalse("No editor-only data (Win64)", Win64->HasEditorOnlyData());
	TestFalse("No editor-only data (PS2)", PS2->HasEditorOnlyData());
	TestTrue("Little-endian", Win64->IsLittleEndian() && PS2->IsLittleEndian());
	TestTrue("Win64 cooks what it runs", Win64->GetCookNote().IsEmpty());
	TestTrue("PS2 says what it converts", PS2->GetCookNote().Contains(TEXT("PSMT8")));

	TArray<FString> Seeds;
	UCookCommandlet::GatherCookSeeds(*Win64, TMap<FString, FString>(), Seeds);
	TestTrue("GameDefaultMap", Seeds.Contains(TEXT("/Engine/Maps/Template_Default")));
	TestTrue("ServerDefaultMap", Seeds.Contains(TEXT("/Engine/Maps/Entry")));
	TestTrue("DefaultMaterialName", Seeds.Contains(TEXT("/Engine/EngineMaterials/M_Default")));
	TestTrue("DefaultTextureName", Seeds.Contains(TEXT("/Engine/EngineResources/DefaultTexture")));
	TestTrue("DirectoriesToAlwaysCook (BaseGame.ini)", Seeds.Contains(TEXT("/Engine/BasicShapes/Sphere")));
	TestFalse("Not a map nothing opens", Seeds.Contains(TEXT("/Engine/Maps/AxisTest")));

	TMap<FString, FString> MapParam;
	MapParam.Add(TEXT("map"), TEXT("/Engine/Maps/AxisTest"));
	UCookCommandlet::GatherCookSeeds(*Win64, MapParam, Seeds);
	TestTrue("-map= adds a map", Seeds.Contains(TEXT("/Engine/Maps/AxisTest")));

	TArray<FString> Packages;
	TestTrue("The closure", UCookCommandlet::CollectDependencies(Seeds, Packages));
	bool bHasMapMesh = false;
	for (const FString& Package : Packages)
	{
		bHasMapMesh |= Package.StartsWith(TEXT("/Engine/Maps/AxisTest/Meshes/"));
	}
	TestTrue("A map's meshes come with it", bHasMapMesh);
	TestEqual("Win64's folder", UCookCommandlet::GetCookedDir(TEXT("Win64")),
		FPaths::ProjectSavedDir() + TEXT("Cooked/Win64/"));
	TestEqual("An engine package's cooked file",
		UCookCommandlet::GetCookedFilename(TEXT("/Engine/Maps/Entry"), TEXT("C:/Cooked/Win64/"), TEXT(".lmap")),
		FString(TEXT("C:/Cooked/Win64/Engine/Content/Maps/Entry.lmap")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdCookClosureTest, "System.LeonEd.Cook.DependencyClosure",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdCookClosureTest::RunTest(const FString& Parameters)
{
	// The closure follows hard imports (asset -> material -> texture) and soft package references (asset -> texture),
	// from the tables alone; what nothing references stays out; a missing soft reference only warns, a missing import
	// fails.
	LeonEdTest::FScopedTestContent Content;
	UTexture2D* Rock = ImportTexture(TEXT("T_Rock"));
	UTexture2D* Soft = ImportTexture(TEXT("T_Soft"));
	TestNotNull("Unused", ImportTexture(TEXT("T_Unused")));
	if (!TestNotNull("Rock", Rock) || !TestNotNull("Soft", Soft))
	{
		return false;
	}
	UMaterial* Material =
		NewObject<UMaterial>(CreatePackage(TEXT("/LeonEdTest/M_Rock")), TEXT("M_Rock"), RF_Public | RF_Standalone);
	Material->BaseColorMap = Rock;
	TestTrue("Material saved", FAssetImportUtils::SavePackage(Material->GetOutermost(), Material));
	TestTrue("Root saved", SaveCookTestAsset(TEXT("/LeonEdTest/DA_Root"), Material, FSoftObjectPath(Soft)));
	TestTrue("A soft reference to nothing",
		SaveCookTestAsset(TEXT("/LeonEdTest/DA_Dangling"), nullptr, FSoftObjectPath(TEXT("/LeonEdTest/Nope.Nope"))));
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);

	TArray<FString> Packages;
	TestTrue("Collected", UCookCommandlet::CollectDependencies({TEXT("/LeonEdTest/DA_Root")}, Packages));
	TArray<FString> Expected = {TEXT("/LeonEdTest/DA_Root"), TEXT("/LeonEdTest/M_Rock"), TEXT("/LeonEdTest/T_Rock"),
		TEXT("/LeonEdTest/T_Soft")};
	TestTrue("The seed, its import, the import's import and the soft reference, sorted", Packages == Expected);
	TestFalse("Nothing references T_Unused", Packages.Contains(TEXT("/LeonEdTest/T_Unused")));

	TestTrue("A dangling soft reference only warns",
		UCookCommandlet::CollectDependencies({TEXT("/LeonEdTest/DA_Dangling")}, Packages));
	TestEqual("Just the asset", Packages.Num(), 1);

	IFileManager::Get().Delete(*FAssetImportUtils::GetPackageFilename(TEXT("/LeonEdTest/T_Rock")));
	AddExpectedError(TEXT("/LeonEdTest/T_Rock does not exist (imported by /LeonEdTest/M_Rock)"));
	TestFalse("A missing import fails", UCookCommandlet::CollectDependencies({TEXT("/LeonEdTest/DA_Root")}, Packages));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdCookedPackagesTest, "System.LeonEd.Cook.CookedPackages",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLeonEdCookedPackagesTest::RunTest(const FString& Parameters)
{
	// An imported map cooked for PS2, twice: the same bytes both times; the world's and the textures' import data are
	// gone and the packages record the platform; the config (no Editor ini) and the shaders are staged beside them.
	LeonEdTest::FScopedTestContent Content;
	const TCHAR* const MapName = TEXT("/LeonEdTest/Maps/MapFixture");
	const FString Fixture = FPaths::ConvertRelativePathToFull(FPaths::Combine(
		FPaths::EngineSourceDir(), TEXT("Developer/MeshUtilities/Private/Tests/Fixtures/MapFixture.gltf")));
	UWorld* World = Cast<UWorld>(
		UImportAssetsCommandlet::ImportAsset(Fixture, MapName, FString(), TEXT("Map"), TMap<FString, FString>()));
	if (!TestNotNull("The map imported", World) || !TestNotNull("with its import data", World->AssetImportData))
	{
		return false;
	}
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);

	const ITargetPlatform* PS2 = FindPlatform(TEXT("PS2"));
	if (!TestNotNull("PS2", PS2))
	{
		return false;
	}
	TArray<FString> Packages;
	TestTrue("The closure", UCookCommandlet::CollectDependencies({MapName}, Packages));
	TestTrue("The map's meshes", Packages.Contains(TEXT("/LeonEdTest/Maps/MapFixture/Meshes/SM_Crate")));
	TestTrue("The map's materials", Packages.Contains(TEXT("/LeonEdTest/Maps/MapFixture/Materials/M_Crate")));

	const FString DirA = LeonEdTest::GetTestDir() + TEXT("CookedA/");
	const FString DirB = LeonEdTest::GetTestDir() + TEXT("CookedB/");
	for (const FString* Dir : {&DirA, &DirB})
	{
		for (const FString& Package : Packages)
		{
			TestTrue(*FString::Printf(TEXT("Cooked %s"), *Package), UCookCommandlet::CookPackage(Package, *PS2, *Dir));
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}
		LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);
	}
	bool bSameBytes = true;
	for (const FString& Package : Packages)
	{
		FString Source;
		(void)FPackageName::DoesPackageExist(Package, nullptr, &Source);
		const FString Extension = FPaths::GetExtension(Source, true);
		const TArray<uint8> A = LeonEdTest::ReadBytes(UCookCommandlet::GetCookedFilename(Package, DirA, Extension));
		const TArray<uint8> B = LeonEdTest::ReadBytes(UCookCommandlet::GetCookedFilename(Package, DirB, Extension));
		bSameBytes &= A.Num() > 0 && A == B;
	}
	TestTrue("Two cooks, the same bytes", bSameBytes);

	const TUniquePtr<FLinkerLoad> Map(FLinkerLoad::CreateLinker(
		nullptr, *UCookCommandlet::GetCookedFilename(MapName, DirA, TEXT(".lmap")), LOAD_None));
	if (TestTrue("The cooked map reads", Map.IsValid()))
	{
		const uint32 Flags = Map->Summary.GetPackageFlags();
		TestTrue("A cooked map",
			(Flags & (PKG_Cooked | PKG_FilterEditorOnly | PKG_ContainsMap)) ==
				uint32(PKG_Cooked | PKG_FilterEditorOnly | PKG_ContainsMap));
		TestEqual("Cooked for PS2", Map->Summary.CookedPlatform, FString(TEXT("PS2")));
		TestTrue("Its world", ExportsClass(*Map, TEXT("World")));
		TestFalse("No import data in the cooked map", ExportsClass(*Map, TEXT("AssetImportData")));
	}
	const TUniquePtr<FLinkerLoad> Texture(FLinkerLoad::CreateLinker(nullptr,
		*UCookCommandlet::GetCookedFilename(TEXT("/LeonEdTest/Maps/MapFixture/Materials/T_MapFixture_Checker"), DirA),
		LOAD_None));
	if (TestTrue("The cooked texture reads", Texture.IsValid()))
	{
		TestTrue("Its texture", ExportsClass(*Texture, TEXT("Texture2D")));
		TestFalse("No import data in the cooked texture", ExportsClass(*Texture, TEXT("AssetImportData")));
	}

	const int32 Staged = UCookCommandlet::StageNonPackageFiles(*PS2, DirA);
	TestTrue("Files staged", Staged > 0);
	TestTrue("The engine config", FPaths::FileExists(DirA + TEXT("Engine/Config/BaseEngine.ini")));
	TestTrue("The packaging settings", FPaths::FileExists(DirA + TEXT("Engine/Config/BaseGame.ini")));
	TestFalse("Not the editor's", FPaths::FileExists(DirA + TEXT("Engine/Config/BaseEditor.ini")));
	TestTrue("The platform's layer", FPaths::FileExists(DirA + TEXT("Engine/Platforms/PS2/Config/PS2Engine.ini")));
	TestTrue("The shaders", FPaths::FileExists(DirA + TEXT("Engine/Shaders/gs_emulator.frag")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdCookPalettedTexturesTest, "System.LeonEd.Cook.PalettedTextures",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLeonEdCookPalettedTexturesTest::RunTest(const FString& Parameters)
{
	// The PS2 cook (and the Win64 one, which cooks the PS2's formats) saves a texture paletted (4 colours: PSMT4,
	// 8 x 8) and leaves the loaded one as it was; the cooked package loads as PF_P4, and the VRAM report counts its
	// page and its CLUT block for the map that uses it.
	LeonEdTest::FScopedTestContent Content;
	UTexture2D* Rock = ImportTexture(TEXT("T_Rock"));
	const ITargetPlatform* PS2 = FindPlatform(TEXT("PS2"));
	const ITargetPlatform* Win64 = FindPlatform(TEXT("Win64"));
	if (!TestNotNull("T_Rock", Rock) || !TestNotNull("PS2", PS2) || !TestNotNull("Win64", Win64))
	{
		return false;
	}
	const FString Dir = LeonEdTest::GetTestDir() + TEXT("CookedPaletted/");
	TArray<FCookedTexture> Textures;
	TestTrue("Cooked", UCookCommandlet::CookPackage(TEXT("/LeonEdTest/T_Rock"), *PS2, Dir, &Textures));
	if (!TestEqual("One texture converted", Textures.Num(), 1))
	{
		return false;
	}
	const FCookedTexture& Cooked = Textures[0];
	TestEqual("PSMT4", int32(Cooked.Format), int32(PF_P4));
	TestTrue("8 x 8 from 2 x 2", Cooked.SizeX == 8 && Cooked.SizeY == 8 && Cooked.SourceSizeX == 2);
	TestEqual("4 colours", Cooked.SourceColors, 4);
	TestEqual("A page and a CLUT block", Cooked.Blocks, 33u);
	TestEqual("The loaded texture is untouched", int32(Rock->GetPixelFormat()), int32(PF_R8G8B8A8));
	TestEqual("its size too", Rock->GetSizeX(), 2);
	TArray<FCookedTexture> Win64Textures;
	TestTrue("Cooked for Win64",
		UCookCommandlet::CookPackage(
			TEXT("/LeonEdTest/T_Rock"), *Win64, LeonEdTest::GetTestDir() + TEXT("CookedWin64/"), &Win64Textures));
	TestTrue("Win64 cooks the PS2's texture (Docs/PLANS/ps2-preview.md V1)",
		Win64Textures.Num() == 1 && Win64Textures[0].Format == PF_P4);

	// The cooked package in the source's place loads paletted.
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);
	const FString Source = FAssetImportUtils::GetPackageFilename(TEXT("/LeonEdTest/T_Rock"));
	TestTrue("Swapped in",
		IFileManager::Get().Copy(*Source, *UCookCommandlet::GetCookedFilename(TEXT("/LeonEdTest/T_Rock"), Dir)));
	const UTexture2D* Loaded = LoadObject<UTexture2D>(nullptr, TEXT("/LeonEdTest/T_Rock.T_Rock"));
	if (TestNotNull("The cooked texture loads", Loaded))
	{
		TestEqual("As PF_P4", int32(Loaded->GetPixelFormat()), int32(PF_P4));
		TestTrue("Valid", Loaded->HasValidPlatformData());
	}

	TMap<FString, TArray<FCookedTexture>> ByPackage;
	ByPackage.Add(TEXT("/LeonEdTest/T_Rock"), Textures);
	TArray<FString> OverBudget;
	const FString Report = UCookCommandlet::MakeVramReport({TEXT("/LeonEdTest/T_Rock")}, {}, ByPackage, 32, OverBudget);
	TestTrue("The report lists the texture", Report.Contains(TEXT("/LeonEdTest/T_Rock.T_Rock 2x2 -> 8x8 PSMT4")));
	TestTrue("33 blocks do not fit in 32", OverBudget.Num() == 1);
	OverBudget.Reset();
	const FString CommonReport =
		UCookCommandlet::MakeVramReport({}, {TEXT("/LeonEdTest/T_Rock")}, ByPackage, 64, OverBudget);
	TestTrue("A common texture",
		CommonReport.Contains(TEXT("Common (the config's default assets, the directories "
								   "always cooked): 1 texture(s), 8 KB")));

	// The RAM report: the same closure, in cooked bytes (rounded up to KB).
	TMap<FString, int64> BytesByPackage;
	BytesByPackage.Add(TEXT("/LeonEdTest/T_Rock"), 3000);
	const FString RamReport = UCookCommandlet::MakeRamReport({TEXT("/LeonEdTest/T_Rock")}, {}, BytesByPackage);
	TestTrue("The RAM report lists the package", RamReport.Contains(TEXT("  /LeonEdTest/T_Rock 3 KB")));
	TestTrue("The map's total",
		RamReport.Contains(TEXT("/LeonEdTest/T_Rock: 1 package(s) of its own, 3 KB; with the common ones 3 KB")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

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
#include "Sound/SoundWave.h"
#include "SpuAdpcm.h"
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

	/** Imports a Size x Size texture of RGB texels into the test mount point (with its editor-only import data). */
	UTexture2D* ImportTexture(const TCHAR* Name, int32 Size = 2, const TArray<uint8>& RGB = CookRGB)
	{
		const FString Source =
			LeonEdTest::WriteSource(FString(Name) + TEXT(".bmp"), LeonEdTest::MakeBmp(Size, Size, RGB));
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
	// Win64 and PS2 both cook the GS's paletted textures; PS2 says what it keeps in the Win64 formats; only Win64
	// compiles shaders; neither keeps editor-only data;
	// the default config seeds the cook with the maps the game opens, the engine's default assets and the folders
	// cooked whole, and not with a map nothing asks for (AxisTest).
	ITargetPlatform* Win64 = FindPlatform(TEXT("win64"));
	ITargetPlatform* PS2 = FindPlatform(TEXT("PS2"));
	if (!TestNotNull("Win64 (case-insensitive)", Win64) || !TestNotNull("PS2", PS2))
	{
		return false;
	}
	TestNull("An unknown platform", FindPlatform(TEXT("Switch")));
	// The development platform is Win64.
	TestTrue("The running platform", GetTargetPlatformManagerRef().GetRunningTargetPlatform() == Win64);
	TestEqual("Win64 config platform", Win64->IniPlatformName(), FString(TEXT("Windows")));
	TestEqual("PS2 config platform", PS2->IniPlatformName(), FString(TEXT("PS2")));
	TestEqual("Win64 records its name", Win64->CookedPlatformName(), FString(TEXT("Win64")));
	TestFalse("No editor-only data (Win64)", Win64->HasEditorOnlyData());
	TestFalse("No editor-only data (PS2)", PS2->HasEditorOnlyData());
	TestTrue("Little-endian", Win64->IsLittleEndian() && PS2->IsLittleEndian());
	TestTrue("Win64 cooks what it runs", Win64->GetCookNote().IsEmpty());
	TestTrue("PS2 says what it converts", PS2->GetCookNote().Contains(TEXT("PSMT8")));
	TArray<FName> Win64WaveFormats;
	TArray<FName> PS2WaveFormats;
	Win64->GetAllWaveFormats(Win64WaveFormats);
	PS2->GetAllWaveFormats(PS2WaveFormats);
	TestTrue("Both play the SPU2's ADPCM (Docs/PLANS/ps2-shipping.md N19)",
		Win64WaveFormats == PS2WaveFormats && PS2WaveFormats.Num() == 1 &&
			PS2WaveFormats[0] == FName(FSpuAdpcm::FormatName));
	TArray<FName> ShaderFormats;
	Win64->GetAllTargetedShaderFormats(ShaderFormats);
	TestEqual("Win64 compiles the GS emulator's shaders", ShaderFormats.Num(), 1);
	ShaderFormats.Reset();
	PS2->GetAllTargetedShaderFormats(ShaderFormats);
	TestEqual("The PS2 has no shaders", ShaderFormats.Num(), 0);

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
	// gone and the packages record the platform; the config (no Editor ini) is staged beside them, and the desktop
	// shaders only for Win64.
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
	TArray<FString> PS2Shaders;
	IFileManager::Get().FindFilesRecursive(PS2Shaders, *(DirA + TEXT("Engine/Shaders")), TEXT("*"), true, false);
	TestEqual("No shaders for the PS2", PS2Shaders.Num(), 0);
	const ITargetPlatform* Win64 = FindPlatform(TEXT("Win64"));
	const FString DirWin64 = LeonEdTest::GetTestDir() + TEXT("CookedWin64/");
	TestTrue("Files staged (Win64)", Win64 != nullptr && UCookCommandlet::StageNonPackageFiles(*Win64, DirWin64) > 0);
	TestTrue("The shaders (Win64)", FPaths::FileExists(DirWin64 + TEXT("Engine/Shaders/gs_emulator.frag")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdCookPalettedTexturesTest, "System.LeonEd.Cook.PalettedTextures",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLeonEdCookPalettedTexturesTest::RunTest(const FString& Parameters)
{
	// The PS2 cook (and the Win64 one, which cooks the PS2's formats) saves a texture paletted (4 colours: PSMT4,
	// 8 x 8) and leaves the loaded one as it was; the cooked package loads as PF_P4, and the VRAM report counts its
	// block (the GS's layout) and its CLUT block for the map that uses it.
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
	TestEqual("A block and a CLUT block", Cooked.Blocks, 2u);
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
	const FString Report = UCookCommandlet::MakeVramReport({TEXT("/LeonEdTest/T_Rock")}, {}, ByPackage, 1, OverBudget);
	TestTrue("The report lists the texture", Report.Contains(TEXT("/LeonEdTest/T_Rock.T_Rock 2x2 -> 8x8 PSMT4")));
	TestTrue("2 blocks do not fit in 1", OverBudget.Num() == 1);
	OverBudget.Reset();
	const FString CommonReport =
		UCookCommandlet::MakeVramReport({}, {TEXT("/LeonEdTest/T_Rock")}, ByPackage, 64, OverBudget);
	TestTrue("A common texture",
		CommonReport.Contains(TEXT("Common (the config's default assets, the directories "
								   "always cooked): 1 texture(s), 1 KB")));

	// The RAM report: the same closure, in cooked bytes (rounded up to KB).
	TMap<FString, int64> BytesByPackage;
	BytesByPackage.Add(TEXT("/LeonEdTest/T_Rock"), 3000);
	FCookBudgets Budgets;
	Budgets.MapRamKB = 1543;
	Budgets.RuntimeBaseKB = 1536;
	Budgets.RuntimeExpansionPercent = 200;
	TArray<FString> RamOverBudget;
	const FString RamReport =
		UCookCommandlet::MakeRamReport({TEXT("/LeonEdTest/T_Rock")}, {}, BytesByPackage, Budgets, RamOverBudget);
	TestTrue("The RAM report lists the package", RamReport.Contains(TEXT("  /LeonEdTest/T_Rock 3 KB")));
	TestTrue("The map's total and its estimate (1536 KB + 2 x 3000 bytes)",
		RamReport.Contains(TEXT("/LeonEdTest/T_Rock: 1 package(s) of its own, 3 KB; with the common ones 3 KB; "
								"estimated at run time 1542 KB")));
	TestEqual("Within 1543 KB", RamOverBudget.Num(), 0);
	Budgets.MapRamKB = 1541;
	(void)UCookCommandlet::MakeRamReport({TEXT("/LeonEdTest/T_Rock")}, {}, BytesByPackage, Budgets, RamOverBudget);
	TestEqual("Over 1541 KB", RamOverBudget.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdCookSpuAdpcmSoundsTest, "System.LeonEd.Cook.SpuAdpcmSounds",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLeonEdCookSpuAdpcmSoundsTest::RunTest(const FString& Parameters)
{
	// The cook saves a sound as its SPU2 ADPCM (Docs/PLANS/ps2-shipping.md N19): a 44.1 kHz stereo .wav becomes mono
	// ADPCM at the default 22 050 Hz; the cooked package loads with it and without the PCM; the SPU2 RAM report counts
	// its bytes and says when a map's sounds do not fit.
	LeonEdTest::FScopedTestContent Content;
	TArray<int16> Samples;
	for (int32 Frame = 0; Frame < 4410; ++Frame)
	{
		const int16 Sample = int16(FMath::RoundToInt(FMath::Sin(float(Frame) * 0.06f) * 8000.0f));
		Samples.Add(Sample);
		Samples.Add(Sample);
	}
	const FString Source = LeonEdTest::WriteSource(TEXT("Audio/Tone.wav"), LeonEdTest::MakeWave(Samples, 2, 44100));
	USoundWave* Sound =
		Cast<USoundWave>(UImportAssetsCommandlet::ImportAsset(Source, TEXT("/LeonEdTest"), FString(), FString(), {}));
	const ITargetPlatform* PS2 = FindPlatform(TEXT("PS2"));
	if (!TestNotNull("S_Tone", Sound) || !TestNotNull("PS2", PS2))
	{
		return false;
	}
	const FString Dir = LeonEdTest::GetTestDir() + TEXT("CookedSounds/");
	TArray<FCookedSound> Sounds;
	TestTrue("Cooked", UCookCommandlet::CookPackage(TEXT("/LeonEdTest/S_Tone"), *PS2, Dir, nullptr, &Sounds));
	if (!TestEqual("One sound", Sounds.Num(), 1))
	{
		return false;
	}
	// 4410 frames at 44.1 kHz: 2205 at 22.05 kHz, 79 blocks (2212 frames).
	const FCookedSound& Cooked = Sounds[0];
	TestTrue("22 050 Hz from 44 100", Cooked.SampleRate == 22050 && Cooked.SourceSampleRate == 44100);
	TestTrue("79 blocks", Cooked.NumFrames == 79 * 28 && Cooked.Bytes == 79 * FSpuAdpcm::BytesPerBlock);
	TestFalse("A one-shot", Cooked.bLooping);

	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);
	const FString SourcePackage = FAssetImportUtils::GetPackageFilename(TEXT("/LeonEdTest/S_Tone"));
	TestTrue("Swapped in",
		IFileManager::Get().Copy(*SourcePackage, *UCookCommandlet::GetCookedFilename(TEXT("/LeonEdTest/S_Tone"), Dir)));
	const USoundWave* Loaded = LoadObject<USoundWave>(nullptr, TEXT("/LeonEdTest/S_Tone.S_Tone"));
	if (TestNotNull("The cooked sound loads", Loaded))
	{
		TestTrue("With its ADPCM",
			Loaded->HasCompressedData() && Loaded->GetCompressedDataSize() == Cooked.Bytes &&
				Loaded->GetCompressedSampleRate() == 22050);
		TestEqual("Without its PCM", Loaded->GetNumFrames(), 0);
	}

	TMap<FString, TArray<FCookedSound>> ByPackage;
	ByPackage.Add(TEXT("/LeonEdTest/S_Tone"), Sounds);
	TArray<FString> OverBudget;
	const FString Report = UCookCommandlet::MakeSoundReport(
		{TEXT("/LeonEdTest/S_Tone")}, {}, ByPackage, FSpuAdpcm::SoundRamBytes, OverBudget);
	TestTrue("The report lists the sound",
		Report.Contains(TEXT("  /LeonEdTest/S_Tone.S_Tone 44100 Hz -> 22050 Hz, 2212 frames, 2 KB")));
	TestEqual("It fits", OverBudget.Num(), 0);
	(void)UCookCommandlet::MakeSoundReport({TEXT("/LeonEdTest/S_Tone")}, {}, ByPackage, 1000, OverBudget);
	TestEqual("1264 bytes do not fit in 1000", OverBudget.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdCookBudgetsTest, "System.LeonEd.Cook.Budgets",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLeonEdCookBudgetsTest::RunTest(const FString& Parameters)
{
	// The hard budgets (Docs/PLANS/ps2-shipping.md N23): the defaults from BaseGame.ini with the platform's limits for
	// 0, the command line over them, and a texture over one fails the whole cook with an error that names it, its
	// figure and the setting.
	const ITargetPlatform* PS2 = FindPlatform(TEXT("PS2"));
	if (!TestNotNull("PS2", PS2))
	{
		return false;
	}
	const FCookBudgets Defaults = FCookBudgets::Load(*PS2, TMap<FString, FString>());
	TestEqual("VRAM: the GS texture arena", Defaults.MapVramKB, 1856);
	TestEqual("RAM: [Core.MemoryBudgets] Total", Defaults.MapRamKB, 24576);
	TestEqual("SPU2 RAM: what audsrv leaves", Defaults.MapSoundRamKB, FSpuAdpcm::SoundRamBytes / 1024);
	TestTrue("Textures: 256, 8 bits", Defaults.MaxTextureSize == 256 && Defaults.MaxTextureBitsPerPixel == 8);
	TestTrue("Meshes", Defaults.MaxMeshTriangles == 4096 && Defaults.MaxMeshBones == 64);
	TMap<FString, FString> Overrides;
	Overrides.Add(TEXT("MaxTextureSize"), TEXT("1000"));
	Overrides.Add(TEXT("MaxMeshBones"), TEXT("20"));
	const FCookBudgets FromCommandLine = FCookBudgets::Load(*PS2, Overrides);
	TestTrue("The command line wins, a texture stays within 256",
		FromCommandLine.MaxTextureSize == 256 && FromCommandLine.MaxMeshBones == 20);

	FCookedPackageInfo Info;
	Info.Meshes.Add({TEXT("/Game/SM_Big.SM_Big"), 5000, 0});
	Info.Meshes.Add({TEXT("/Game/SK_Many.SK_Many"), 100, 80});
	TArray<FString> BudgetErrors;
	Defaults.CheckAssets(Info, BudgetErrors);
	TestTrue("A mesh over its triangles and one over its bones",
		BudgetErrors.Num() == 2 &&
			BudgetErrors[0] ==
				TEXT("/Game/SM_Big.SM_Big has 5000 triangles, over [/Script/LeonEd.CookSettings] "
					 "MaxMeshTriangles=4096") &&
			BudgetErrors[1] ==
				TEXT("/Game/SK_Many.SK_Many has 80 bones, over [/Script/LeonEd.CookSettings] MaxMeshBones=64"));

	// A 32 x 32 texture of 64 colours cooks to PSMT8 32 x 32: over a 16 texel budget and a 4 bit one, the cook fails.
	LeonEdTest::FScopedTestContent Content;
	TArray<uint8> Colors;
	for (int32 Texel = 0; Texel < 32 * 32; ++Texel)
	{
		Colors.Add(uint8((Texel % 64) * 4));
		Colors.Add(uint8(0));
		Colors.Add(uint8(200));
	}
	if (!TestNotNull("T_Colors", ImportTexture(TEXT("T_Colors"), 32, Colors)))
	{
		return false;
	}
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);
	// Rooted: the cook collects garbage between packages.
	UCookCommandlet* Cook = NewObject<UCookCommandlet>();
	Cook->AddToRoot();
	TestEqual("Within the budgets the cook succeeds",
		Cook->Main(TEXT("-TargetPlatform=PS2 -package=/LeonEdTest/T_Colors -full")), 0);
	AddExpectedError(TEXT("Cook: over budget: /LeonEdTest/T_Colors.T_Colors is 32x32, over "
						  "[/Script/LeonEd.CookSettings] MaxTextureSize=16"));
	AddExpectedError(TEXT("Cook: over budget: /LeonEdTest/T_Colors.T_Colors is 8 bits a texel (PSMT8, 64 colours), "
						  "over [/Script/LeonEd.CookSettings] MaxTextureBitsPerPixel=4"));
	TestEqual("Over them it fails",
		Cook->Main(TEXT(
			"-TargetPlatform=PS2 -package=/LeonEdTest/T_Colors -full -MaxTextureSize=16 -MaxTextureBitsPerPixel=4")),
		1);
	Cook->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdCookCacheTest, "System.LeonEd.Cook.Cache",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLeonEdCookCacheTest::RunTest(const FString& Parameters)
{
	// The incremental cook (N23): a package's key follows its source, its imports' sources, the cooker and the
	// platform; the first cook misses and fills the cache, the next one copies the same bytes and the same report
	// data from it, and a changed import (the material's texture) invalidates the importer too.
	LeonEdTest::FScopedTestContent Content;
	UTexture2D* Rock = ImportTexture(TEXT("T_Rock"));
	if (!TestNotNull("T_Rock", Rock))
	{
		return false;
	}
	UMaterial* Material =
		NewObject<UMaterial>(CreatePackage(TEXT("/LeonEdTest/M_Rock")), TEXT("M_Rock"), RF_Public | RF_Standalone);
	Material->BaseColorMap = Rock;
	TestTrue("Material saved", FAssetImportUtils::SavePackage(Material->GetOutermost(), Material));
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);
	const ITargetPlatform* PS2 = FindPlatform(TEXT("PS2"));
	const ITargetPlatform* Win64 = FindPlatform(TEXT("Win64"));
	if (!TestNotNull("PS2", PS2) || !TestNotNull("Win64", Win64))
	{
		return false;
	}
	const TCHAR* const TextureName = TEXT("/LeonEdTest/T_Rock");
	const TCHAR* const MaterialName = TEXT("/LeonEdTest/M_Rock");
	FCookKeyCache Keys;
	const FString TextureKey = UCookCommandlet::GetCookKey(TextureName, *PS2, Keys);
	const FString MaterialKey = UCookCommandlet::GetCookKey(MaterialName, *PS2, Keys);
	TestTrue("Keys", TextureKey.Len() == 40 && MaterialKey.Len() == 40 && TextureKey != MaterialKey);
	TestEqual("The same again", UCookCommandlet::GetCookKey(TextureName, *PS2, Keys), TextureKey);
	FCookKeyCache OtherKeys;
	TestNotEqual(
		"Another platform, another key", UCookCommandlet::GetCookKey(TextureName, *Win64, OtherKeys), TextureKey);

	const FString CacheDir = LeonEdTest::GetTestDir() + TEXT("CookCache/");
	const FString FullDir = LeonEdTest::GetTestDir() + TEXT("CookedFull/");
	const FString IterDir = LeonEdTest::GetTestDir() + TEXT("CookedIterative/");
	FCookedPackageInfo Full;
	FCookedPackageInfo Iterative;
	bool bFromCache = true;
	TestTrue("A full cook",
		UCookCommandlet::CookPackageCached(TextureName, *PS2, FullDir, CacheDir, TextureKey, false, Full, bFromCache));
	TestFalse("cooks", bFromCache);
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestTrue("An iterative cook",
		UCookCommandlet::CookPackageCached(
			TextureName, *PS2, IterDir, CacheDir, TextureKey, true, Iterative, bFromCache));
	TestTrue("copies from the cache", bFromCache);
	const TArray<uint8> FullBytes = LeonEdTest::ReadBytes(UCookCommandlet::GetCookedFilename(TextureName, FullDir));
	const TArray<uint8> IterBytes = LeonEdTest::ReadBytes(UCookCommandlet::GetCookedFilename(TextureName, IterDir));
	TestTrue("The same bytes as the full cook", FullBytes.Num() > 0 && FullBytes == IterBytes);
	TestTrue("The same report data",
		Iterative.Textures.Num() == 1 && Full.Textures.Num() == 1 &&
			Iterative.Textures[0].Name == Full.Textures[0].Name &&
			Iterative.Textures[0].Blocks == Full.Textures[0].Blocks &&
			Iterative.Textures[0].Format == Full.Textures[0].Format &&
			Iterative.Textures[0].SourceColors == Full.Textures[0].SourceColors);
	TestTrue("A key the cache has not seen misses",
		UCookCommandlet::CookPackageCached(
			MaterialName, *PS2, IterDir, CacheDir, MaterialKey, true, Iterative, bFromCache) &&
			!bFromCache);
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestTrue("and hits the next time",
		UCookCommandlet::CookPackageCached(
			MaterialName, *PS2, IterDir, CacheDir, MaterialKey, true, Iterative, bFromCache) &&
			bFromCache);

	// The texture's source changes: its key and its importer's change, and the cache misses both.
	const TArray<uint8> OtherRGB = {200, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120};
	TestNotNull("Reimported", ImportTexture(TEXT("T_Rock"), 2, OtherRGB));
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);
	FCookKeyCache NewKeys;
	const FString NewTextureKey = UCookCommandlet::GetCookKey(TextureName, *PS2, NewKeys);
	const FString NewMaterialKey = UCookCommandlet::GetCookKey(MaterialName, *PS2, NewKeys);
	TestTrue("Both keys change", NewTextureKey != TextureKey && NewMaterialKey != MaterialKey);
	TestTrue("The texture is cooked again",
		UCookCommandlet::CookPackageCached(
			TextureName, *PS2, IterDir, CacheDir, NewTextureKey, true, Iterative, bFromCache) &&
			!bFromCache);
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	TestTrue("and so is the material",
		UCookCommandlet::CookPackageCached(
			MaterialName, *PS2, IterDir, CacheDir, NewMaterialKey, true, Iterative, bFromCache) &&
			!bFromCache);
	CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdCookStripConfigTest, "System.LeonEd.Cook.StripConfig",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdCookStripConfigTest::RunTest(const FString& Parameters)
{
	// A staged config keeps what the game reads (N23): no comments, no blank lines, no packaging or cook sections.
	const FString Source =
		TEXT("; A comment\n\n[/Script/Engine.Engine]\n; Why\nGameName=Leon  \n\n"
			 "[/Script/UnrealEd.ProjectPackagingSettings]\n+DirectoriesToAlwaysCook=(Path=\"/Game/X\")\n"
			 "[/Script/LeonEd.CookSettings]\nMaxTextureSize=128\n[Core.Log]\nLogPakFile=Log\n");
	TestEqual("Stripped", UCookCommandlet::StripConfigForTarget(Source),
		FString(TEXT("[/Script/Engine.Engine]\nGameName=Leon\n[Core.Log]\nLogPakFile=Log\n")));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

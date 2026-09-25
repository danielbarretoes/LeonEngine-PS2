#include "AssetImportUtils.h"
#include "CommandletHelpers.h"
#include "Commandlets/CookCommandlet.h"
#include "Commandlets/ImportAssetsCommandlet.h"
#include "Commandlets/MigrateLegacyContentCommandlet.h"
#include "Commandlets/ResavePackagesCommandlet.h"
#include "Commandlets/ValidateAssetsCommandlet.h"
#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"
#include "EditorReimportHandler.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/Factory.h"
#include "Level/LegacyAssetKeys.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Sound/SoundWave.h"
#include "Templates/UniquePtr.h"
#include "Tests/LeonEdTestUtils.h"
#include "UObject/LinkerLoad.h"

#if WITH_DEV_AUTOMATION_TESTS

// The LeonEd commandlets, called as LeonCook calls them (Main with the command line after -run=): import, import
// lists, reimport (gate G5: the same source saves the same bytes), resave, validation, the minimal cook and the legacy
// content migration.

namespace
{

	/** Runs a commandlet the way LeonCook does: found by name, made in the transient package, Main(Params). */
	int32 RunCommandlet(const TCHAR* Name, const FString& Params)
	{
		UClass* Class = CommandletHelpers::FindCommandletClass(Name);
		if (Class == nullptr)
		{
			return -1;
		}
		UCommandlet* Commandlet = NewObject<UCommandlet>(GetTransientPackage(), Class);
		return Commandlet->Main(Params);
	}

	/** The file of a package of the test mount point. */
	FString PackageFile(const TCHAR* PackageName)
	{
		return FAssetImportUtils::GetPackageFilename(PackageName);
	}

	const TArray<uint8> TestRGB = {255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255};

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdCommandletLookupTest, "System.LeonEd.Commandlets.FoundByName",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdCommandletLookupTest::RunTest(const FString& Parameters)
{
	// -run=<Name> names U<Name>Commandlet (or U<Name>), found through reflection.
	TestTrue("ImportAssets",
		CommandletHelpers::FindCommandletClass(TEXT("ImportAssets")) == UImportAssetsCommandlet::StaticClass());
	TestTrue("Full class name",
		CommandletHelpers::FindCommandletClass(TEXT("CookCommandlet")) == UCookCommandlet::StaticClass());
	TestTrue("Case-insensitive",
		CommandletHelpers::FindCommandletClass(TEXT("resavepackages")) == UResavePackagesCommandlet::StaticClass());
	TestNull("Unknown", CommandletHelpers::FindCommandletClass(TEXT("Nope")));
	TestNull("Abstract base", CommandletHelpers::FindCommandletClass(TEXT("Commandlet")));
	TArray<UClass*> Classes;
	CommandletHelpers::GetCommandletClasses(Classes);
	for (UClass* Expected : {UCookCommandlet::StaticClass(), UImportAssetsCommandlet::StaticClass(),
			 UMigrateLegacyContentCommandlet::StaticClass(), UResavePackagesCommandlet::StaticClass(),
			 UValidateAssetsCommandlet::StaticClass()})
	{
		TestTrue(*FString::Printf(TEXT("%s listed"), *Expected->GetName()), Classes.Contains(Expected));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdImportAssetsTest, "System.LeonEd.Commandlets.ImportAssets",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdImportAssetsTest::RunTest(const FString& Parameters)
{
	// -source/-dest imports and saves one file, named with its prefix; -importlist imports every section with its
	// settings; importing over an asset reimports it in place.
	LeonEdTest::FScopedTestContent Content;
	const FString Cube = FPaths::ConvertRelativePathToFull(
		FPaths::Combine(FPaths::EngineSourceDir(), TEXT("Developer/MeshUtilities/Private/Tests/Fixtures/Cube.obj")));
	TestEqual("-source -dest",
		RunCommandlet(TEXT("ImportAssets"), TEXT("-source=\"") + Cube + TEXT("\" -dest=/LeonEdTest/Meshes")), 0);
	TestTrue("Saved with its prefix", FPaths::FileExists(LeonEdTest::GetContentDir() + TEXT("Meshes/SM_Cube.lasset")));

	(void)LeonEdTest::WriteSource(TEXT("Textures/Grid.bmp"), LeonEdTest::MakeBmp(2, 2, TestRGB));
	(void)LeonEdTest::WriteSource(TEXT("Audio/Click.wav"), LeonEdTest::MakeWave(TArray<int16>({1, 2, 3, 4}), 1, 22050));
	const FString List = LeonEdTest::WriteSource(TEXT("ImportList.ini"),
		FString(TEXT("[Grid]\nSource=Textures/Grid.bmp\nDest=/LeonEdTest/Textures\nName=T_Grid_N\n"
					 "ColorSpaceMode=SRGB\n\n[Click]\nSource=Audio/Click.wav\nDest=/LeonEdTest/Audio\n")));
	TestEqual("-importlist", RunCommandlet(TEXT("ImportAssets"), TEXT("-importlist=\"") + List + TEXT("\"")), 0);
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);

	UTexture2D* Grid = LoadObject<UTexture2D>(nullptr, TEXT("/LeonEdTest/Textures/T_Grid_N.T_Grid_N"));
	if (TestNotNull("The list's texture loads", Grid))
	{
		TestEqual("Its setting won over the _N name", static_cast<int32>(Grid->SRGB), 1);
		TestEqual(
			"Setting recorded", Grid->AssetImportData->ImportSettings.FindRef(TEXT("ColorSpaceMode")), FString("SRGB"));
		TestEqual("Source recorded", Grid->AssetImportData->SourceData.SourceFiles[0].RelativeFilename,
			FString("SourceArt/Textures/Grid.bmp"));
	}
	USoundWave* Click = LoadObject<USoundWave>(nullptr, TEXT("/LeonEdTest/Audio/S_Click.S_Click"));
	TestTrue("The list's sound loads", Click != nullptr && Click->SampleRate == 22050);

	// A material references the texture; importing the texture again keeps the object, so the reference holds.
	UMaterial* User = NewObject<UMaterial>(CreatePackage(TEXT("/LeonEdTest/M_User")), TEXT("M_User"), RF_Public);
	User->BaseColorMap = Grid;
	UObject* Again = UImportAssetsCommandlet::ImportAsset(LeonEdTest::GetSourceDir() + TEXT("Textures/Grid.bmp"),
		TEXT("/LeonEdTest/Textures"), TEXT("T_Grid_N"), FString(), TMap<FString, FString>());
	TestTrue("Imported over in place", Again == Grid && User->BaseColorMap == Grid);

	AddExpectedError(TEXT("does not exist"), 1);
	TestEqual("A missing source fails",
		RunCommandlet(TEXT("ImportAssets"), TEXT("-source=Missing.png -dest=/LeonEdTest")), 1);
	AddExpectedError(TEXT("not a package under a mount point"), 1);
	TestEqual("A destination outside the mount points fails",
		RunCommandlet(TEXT("ImportAssets"), TEXT("-source=\"") + Cube + TEXT("\" -dest=/Nowhere")), 1);
	AddExpectedError(TEXT("bad import settings"), 1);
	TestEqual("An unknown setting fails",
		RunCommandlet(TEXT("ImportAssets"), TEXT("-source=\"") + Cube + TEXT("\" -dest=/LeonEdTest -Bogus=1")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdReimportTest, "System.LeonEd.Commandlets.ReimportIsReproducible",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdReimportTest::RunTest(const FString& Parameters)
{
	// Gate G5 in small: -reimport -all over unchanged sources writes the same bytes; a changed source changes the
	// asset and its MD5; an asset whose source is gone is skipped.
	LeonEdTest::FScopedTestContent Content;
	const FString Texture = LeonEdTest::WriteSource(TEXT("T_Wall.bmp"), LeonEdTest::MakeBmp(2, 2, TestRGB));
	const FString Mesh = LeonEdTest::WriteSource(
		TEXT("Tri.obj"), FString(TEXT("v 0 0 0\nv 1 0 0\nv 0 1 0\nvn 0 0 1\nf 1//1 2//1 3//1\n")));
	const FString Gone = LeonEdTest::WriteSource(TEXT("T_Gone.bmp"), LeonEdTest::MakeBmp(2, 2, TestRGB));
	TestNotNull(
		"Texture", UImportAssetsCommandlet::ImportAsset(Texture, TEXT("/LeonEdTest"), FString(), FString(), {}));
	TestNotNull("Mesh", UImportAssetsCommandlet::ImportAsset(Mesh, TEXT("/LeonEdTest"), FString(), FString(), {}));
	TestNotNull("Gone", UImportAssetsCommandlet::ImportAsset(Gone, TEXT("/LeonEdTest"), FString(), FString(), {}));
	IFileManager::Get().Delete(*Gone);
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);

	const TArray<uint8> TextureBytes = LeonEdTest::ReadBytes(PackageFile(TEXT("/LeonEdTest/T_Wall")));
	const TArray<uint8> MeshBytes = LeonEdTest::ReadBytes(PackageFile(TEXT("/LeonEdTest/SM_Tri")));
	TArray<FString> Packages = {TEXT("/LeonEdTest/SM_Tri"), TEXT("/LeonEdTest/T_Gone"), TEXT("/LeonEdTest/T_Wall")};
	int32 Reimported = 0;
	TestEqual("Reimported without failures", UImportAssetsCommandlet::ReimportPackages(Packages, &Reimported), 0);
	TestEqual("Two had a source", Reimported, 2);
	TestTrue("The texture's bytes did not change",
		TextureBytes.Num() > 0 && LeonEdTest::ReadBytes(PackageFile(TEXT("/LeonEdTest/T_Wall"))) == TextureBytes);
	TestTrue("The mesh's bytes did not change",
		MeshBytes.Num() > 0 && LeonEdTest::ReadBytes(PackageFile(TEXT("/LeonEdTest/SM_Tri"))) == MeshBytes);

	const TArray<uint8> Other = {9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9};
	(void)LeonEdTest::WriteSource(TEXT("T_Wall.bmp"), LeonEdTest::MakeBmp(2, 2, Other));
	TestEqual(
		"-reimport -package", RunCommandlet(TEXT("ImportAssets"), TEXT("-reimport -package=/LeonEdTest/T_Wall")), 0);
	const TArray<uint8> Changed = LeonEdTest::ReadBytes(PackageFile(TEXT("/LeonEdTest/T_Wall")));
	TestTrue("A changed source changes the asset", Changed.Num() > 0 && Changed != TextureBytes);
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);
	const UTexture2D* Wall = LoadObject<UTexture2D>(nullptr, TEXT("/LeonEdTest/T_Wall.T_Wall"));
	TestTrue("and its MD5",
		Wall != nullptr && Wall->AssetImportData->GetFirstFileHash() == UAssetImportData::HashFile(Texture));
	TestTrue("The manager can reimport it", FReimportManager::Instance()->CanReimport(const_cast<UTexture2D*>(Wall)));
	AddExpectedError(TEXT("-reimport needs"), 1);
	TestEqual("-reimport alone", RunCommandlet(TEXT("ImportAssets"), TEXT("-reimport")), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdResaveValidateCookTest, "System.LeonEd.Commandlets.ResaveValidateCook",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdResaveValidateCookTest::RunTest(const FString& Parameters)
{
	// ResavePackages writes the bytes back unchanged; ValidateAssets passes valid packages and reports an import whose
	// package is gone; the cook drops the editor-only import data.
	LeonEdTest::FScopedTestContent Content;
	const FString Texture = LeonEdTest::WriteSource(TEXT("T_Rock.bmp"), LeonEdTest::MakeBmp(2, 2, TestRGB));
	UTexture2D* Rock =
		Cast<UTexture2D>(UImportAssetsCommandlet::ImportAsset(Texture, TEXT("/LeonEdTest"), FString(), FString(), {}));
	UMaterial* Material =
		NewObject<UMaterial>(CreatePackage(TEXT("/LeonEdTest/M_Rock")), TEXT("M_Rock"), RF_Public | RF_Standalone);
	Material->BaseColorMap = Rock;
	TestTrue("Material saved", FAssetImportUtils::SavePackage(Material->GetOutermost(), Material));
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);

	const TArray<uint8> Before = LeonEdTest::ReadBytes(PackageFile(TEXT("/LeonEdTest/M_Rock")));
	TestEqual("ResavePackages", RunCommandlet(TEXT("ResavePackages"), TEXT("-packagefolder=/LeonEdTest")), 0);
	TestTrue(
		"Same bytes", Before.Num() > 0 && LeonEdTest::ReadBytes(PackageFile(TEXT("/LeonEdTest/M_Rock"))) == Before);
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);
	TestEqual("Valid", RunCommandlet(TEXT("ValidateAssets"), TEXT("-packagefolder=/LeonEdTest")), 0);

	TestEqual("Cook", RunCommandlet(TEXT("Cook"), TEXT("-packagefolder=/LeonEdTest -TargetPlatform=Test")), 0);
	const FString Cooked = UCookCommandlet::GetCookedFilename(TEXT("/LeonEdTest/T_Rock"), TEXT("Test"));
	TestTrue("In Saved/Cooked/<Platform>/<Mount>/Content",
		Cooked.EndsWith(TEXT("Saved/Cooked/Test/LeonEdTest/Content/T_Rock.lasset")));
	const TUniquePtr<FLinkerLoad> Tables(FLinkerLoad::CreateLinker(nullptr, *Cooked, LOAD_None));
	if (TestTrue("The cooked package reads", Tables.IsValid()))
	{
		TestTrue("Cooked, editor-only data filtered",
			(Tables->Summary.GetPackageFlags() & (PKG_Cooked | PKG_FilterEditorOnly)) ==
				static_cast<uint32>(PKG_Cooked | PKG_FilterEditorOnly));
		bool bHasImportData = false;
		for (int32 Index = 0; Index < Tables->ExportMap.Num(); ++Index)
		{
			bHasImportData |= Tables->GetExportClassName(Index) == FName(TEXT("AssetImportData"));
		}
		TestFalse("No import data", bHasImportData);
		TestEqual("Just the texture", Tables->ExportMap.Num(), 1);
	}
	IFileManager::Get().DeleteDirectory(*(FPaths::ProjectSavedDir() + TEXT("Cooked/Test")), false, true);
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);

	IFileManager::Get().Delete(*PackageFile(TEXT("/LeonEdTest/T_Rock")));
	AddExpectedError(TEXT("does not resolve"), 2);
	TestEqual("A missing import", UValidateAssetsCommandlet::ValidatePackage(TEXT("/LeonEdTest/M_Rock")) > 0, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdMigrateLegacyContentTest, "System.LeonEd.Commandlets.MigrateLegacyContent",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FLeonEdMigrateLegacyContentTest::RunTest(const FString& Parameters)
{
	// A legacy content folder outside every mount point gets one named after it; its images and sounds are imported,
	// its .lmat materials converted with their maps resolved to the new packages, and the content keys of the old
	// files resolve to them.
	LeonEdTest::FScopedTestContent Content;
	const FString Legacy = LeonEdTest::GetTestDir() + TEXT("LegacyPack/");
	(void)FFileHelper::SaveArrayToFile(LeonEdTest::MakeBmp(2, 2, TestRGB), *(Legacy + TEXT("Textures/Bricks.bmp")));
	(void)FFileHelper::SaveArrayToFile(
		LeonEdTest::MakeWave(TArray<int16>({7, 8}), 1, 8000), *(Legacy + TEXT("Audio/Hit.wav")));
	(void)FFileHelper::SaveStringToFile(
		FString(TEXT(
			"[Parameters]\nBaseColor=0.8,0.2,0.15\nRoughness=0.5\n[Textures]\nBaseColorMap=Textures/Bricks.bmp\n")),
		*(Legacy + TEXT("Materials/Red.lmat")));
	(void)FFileHelper::SaveStringToFile(FString(TEXT("not legacy content")), *(Legacy + TEXT("README.md")));

	TestEqual("Migrated", RunCommandlet(TEXT("MigrateLegacyContent"), TEXT("-source=\"") + Legacy + TEXT("\"")), 0);
	const FString Root = FLegacyAssetKeys::MountContentDirectory(Legacy);
	TestEqual("A mount point named after the folder", Root, FString("/LegacyPack"));
	TestTrue("T_Bricks next to its source", FPaths::FileExists(Legacy + TEXT("Textures/T_Bricks.lasset")));
	TestTrue("S_Hit", FPaths::FileExists(Legacy + TEXT("Audio/S_Hit.lasset")));
	TestTrue("M_Red", FPaths::FileExists(Legacy + TEXT("Materials/M_Red.lasset")));
	LeonEdTest::DestroyPackagesUnder(TEXT("/LegacyPack/"));

	const FString RedPath = FLegacyAssetKeys::ResolveKey(Root, TEXT("Materials/Red.lmat"));
	TestEqual("The .lmat key resolves", RedPath, FString("/LegacyPack/Materials/M_Red.M_Red"));
	const UMaterial* Red = LoadObject<UMaterial>(nullptr, *RedPath);
	if (TestNotNull("M_Red loads", Red))
	{
		TestTrue("Its parameters",
			Red->BaseColor.Equals(FLinearColor(0.8f, 0.2f, 0.15f, 1.0f)) && FMath::IsNearlyEqual(Red->Roughness, 0.5f));
		TestTrue("Its map is the migrated texture",
			Red->BaseColorMap != nullptr &&
				Red->BaseColorMap->GetPathName() == TEXT("/LegacyPack/Textures/T_Bricks.T_Bricks"));
		TestNull("No import data on a material", UFactory::GetAssetImportData(const_cast<UMaterial*>(Red)));
	}
	LeonEdTest::DestroyPackagesUnder(TEXT("/LegacyPack/"));
	FPackageName::UnRegisterMountPoint(TEXT("/LegacyPack/"), Legacy);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

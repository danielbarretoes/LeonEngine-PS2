#include "Animation/AimOffsetBlendSpace1D.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/Skeleton.h"
#include "AssetImportUtils.h"
#include "CommandletHelpers.h"
#include "Commandlets/CookCommandlet.h"
#include "Commandlets/ImportAssetsCommandlet.h"
#include "Commandlets/ResavePackagesCommandlet.h"
#include "Commandlets/ValidateAssetsCommandlet.h"
#include "CoreMinimal.h"
#include "EditorFramework/AssetImportData.h"
#include "EditorReimportHandler.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Factories/Factory.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Sound/SoundWave.h"
#include "Templates/UniquePtr.h"
#include "Tests/LeonEdTestUtils.h"
#include "UObject/LinkerLoad.h"

#if WITH_DEV_AUTOMATION_TESTS

// The LeonEd commandlets, called as LeonCook calls them (Main with the command line after -run=): import, import
// lists, reimport (gate G5: the same source saves the same bytes), resave, validation and the cook (CookTests.cpp has
// its closure and its output).

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

	/** A glTF test fixture (MeshUtilities' Tests/Fixtures, written by MakeSkinnedFixture.py). */
	FString FixturePath(const TCHAR* FileName)
	{
		return FPaths::ConvertRelativePathToFull(FPaths::Combine(
			FPaths::EngineSourceDir(), TEXT("Developer/MeshUtilities/Private/Tests/Fixtures"), FileName));
	}

	/** A one-triangle .gltf, its buffer a data URI: (0, 0, 0), (1, 0, 0), (0, 1, 0) m. */
	const TCHAR* const TriangleGltf = TEXT("{\"asset\":{\"version\":\"2.0\"},\"buffers\":[{\"byteLength\":36,\"uri\":")
		TEXT("\"data:application/octet-stream;base64,AAAAAAAAAAAAAAAAAACAPwAAAAAAAAAAAAAAAAAAgD8AAAAA\"}],")
			TEXT("\"bufferViews\":[{\"buffer\":0,\"byteLength\":36}],\"accessors\":[{\"bufferView\":0,")
				TEXT("\"componentType\":5126,\"count\":3,\"type\":\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]}],")
					TEXT("\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0}}]}]}");

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
			 UResavePackagesCommandlet::StaticClass(), UValidateAssetsCommandlet::StaticClass()})
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
	const FString Cube = FixturePath(TEXT("Cube.glb"));
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
	const FString Mesh = LeonEdTest::WriteSource(TEXT("Tri.gltf"), FString(TriangleGltf));
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

	AddExpectedError(TEXT("unknown -TargetPlatform=Test"));
	TestEqual(
		"An unknown platform", RunCommandlet(TEXT("Cook"), TEXT("-packagefolder=/LeonEdTest -TargetPlatform=Test")), 1);
	TestEqual("Cook", RunCommandlet(TEXT("Cook"), TEXT("-packagefolder=/LeonEdTest -TargetPlatform=PS2")), 0);
	const FString Cooked =
		UCookCommandlet::GetCookedFilename(TEXT("/LeonEdTest/T_Rock"), UCookCommandlet::GetCookedDir(TEXT("PS2")));
	TestTrue("In Saved/Cooked/<Platform>/<Mount>/Content",
		Cooked.EndsWith(TEXT("Saved/Cooked/PS2/LeonEdTest/Content/T_Rock.lasset")));
	const TUniquePtr<FLinkerLoad> Tables(FLinkerLoad::CreateLinker(nullptr, *Cooked, LOAD_None));
	if (TestTrue("The cooked package reads", Tables.IsValid()))
	{
		TestTrue("Cooked, editor-only data filtered",
			(Tables->Summary.GetPackageFlags() & (PKG_Cooked | PKG_FilterEditorOnly)) ==
				static_cast<uint32>(PKG_Cooked | PKG_FilterEditorOnly));
		TestEqual("Cooked for the target platform", Tables->Summary.CookedPlatform, FString(TEXT("PS2")));
		bool bHasImportData = false;
		for (int32 Index = 0; Index < Tables->ExportMap.Num(); ++Index)
		{
			bHasImportData |= Tables->GetExportClassName(Index) == FName(TEXT("AssetImportData"));
		}
		TestFalse("No import data", bHasImportData);
		TestEqual("Just the texture", Tables->ExportMap.Num(), 1);
	}
	IFileManager::Get().DeleteDirectory(*UCookCommandlet::GetCookedDir(TEXT("PS2")), false, true);
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);

	IFileManager::Get().Delete(*PackageFile(TEXT("/LeonEdTest/T_Rock")));
	AddExpectedError(TEXT("does not resolve"), 2);
	TestEqual("A missing import", UValidateAssetsCommandlet::ValidatePackage(TEXT("/LeonEdTest/M_Rock")) > 0, true);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdGltfSkeletalImportListTest,
	"System.LeonEd.Commandlets.GltfSkeletalDeterministic",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLeonEdGltfSkeletalImportListTest::RunTest(const FString& Parameters)
{
	// An ImportList.ini imports a .glb as a skeletal mesh (Type=SkeletalMesh: SK_, SKEL_, the material and its embedded
	// texture) and its animations (Type=Animation on that skeleton: an A_ per glTF animation). Importing again, and
	// reimporting, writes the same bytes (gate G5).
	LeonEdTest::FScopedTestContent Content;
	const FString Arm = FixturePath(TEXT("SkinnedArm.glb"));
	const FString List = LeonEdTest::WriteSource(TEXT("ImportList.ini"),
		TEXT("[SK_Arm]\nSource=") + Arm +
			TEXT("\nDest=/LeonEdTest/Arm\nType=SkeletalMesh\n\n[ArmAnimations]\nSource=") + Arm +
			TEXT("\nDest=/LeonEdTest/Arm\nType=Animation\nSkeleton=/LeonEdTest/Arm/SKEL_SkinnedArm.SKEL_SkinnedArm\n"));
	TestEqual("-importlist", RunCommandlet(TEXT("ImportAssets"), TEXT("-importlist=\"") + List + TEXT("\"")), 0);
	const TCHAR* const Packages[] = {TEXT("/LeonEdTest/Arm/SK_SkinnedArm"), TEXT("/LeonEdTest/Arm/SKEL_SkinnedArm"),
		TEXT("/LeonEdTest/Arm/M_ArmSkin"), TEXT("/LeonEdTest/Arm/T_ArmSkin_D"), TEXT("/LeonEdTest/Arm/A_Wave"),
		TEXT("/LeonEdTest/Arm/A_Grip")};
	TArray<TArray<uint8>> First;
	for (const TCHAR* Package : Packages)
	{
		First.Add(LeonEdTest::ReadBytes(PackageFile(Package)));
		TestTrue(*FString::Printf(TEXT("%s saved"), Package), First.Last().Num() > 0);
	}
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);

	const USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/LeonEdTest/Arm/SK_SkinnedArm.SK_SkinnedArm"));
	const UAnimSequence* Wave = LoadObject<UAnimSequence>(nullptr, TEXT("/LeonEdTest/Arm/A_Wave.A_Wave"));
	TestTrue("They load, the clip on the mesh's skeleton",
		Mesh != nullptr && Wave != nullptr && Mesh->Skeleton != nullptr && Wave->GetSkeleton() == Mesh->Skeleton &&
			Mesh->HasValidRenderData() && Wave->GetNumberOfTracks() == 3);
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);

	TestEqual("-importlist again", RunCommandlet(TEXT("ImportAssets"), TEXT("-importlist=\"") + List + TEXT("\"")), 0);
	for (int32 Index = 0; Index < int32(UE_ARRAY_COUNT(Packages)); ++Index)
	{
		TestTrue(*FString::Printf(TEXT("%s: the same bytes"), Packages[Index]),
			LeonEdTest::ReadBytes(PackageFile(Packages[Index])) == First[Index]);
	}
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);

	int32 Reimported = 0;
	TestEqual("Reimported",
		UImportAssetsCommandlet::ReimportPackages(
			{TEXT("/LeonEdTest/Arm/SK_SkinnedArm"), TEXT("/LeonEdTest/Arm/A_Wave"), TEXT("/LeonEdTest/Arm/A_Grip")},
			&Reimported),
		0);
	TestEqual("Three with a source", Reimported, 3);
	for (int32 Index = 0; Index < int32(UE_ARRAY_COUNT(Packages)); ++Index)
	{
		TestTrue(*FString::Printf(TEXT("%s: the same bytes after a reimport"), Packages[Index]),
			LeonEdTest::ReadBytes(PackageFile(Packages[Index])) == First[Index]);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLeonEdAnimationAssetsFromListTest,
	"System.LeonEd.Commandlets.AnimationAssetsFromImportList",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)

bool FLeonEdAnimationAssetsFromListTest::RunTest(const FString& Parameters)
{
	// Blend spaces, an aim offset and a montage have no source file: ImportList.ini sections describe them from the A_
	// the same list imports (Docs/PLANS/ps2-shipping.md N25, Docs/TOOLS.md). Making them again writes the same bytes.
	LeonEdTest::FScopedTestContent Content;
	const FString Arm = FixturePath(TEXT("SkinnedArm.glb"));
	const FString List = LeonEdTest::WriteSource(TEXT("AnimationList.ini"),
		TEXT("[SK_Arm]\nSource=") + Arm +
			TEXT("\nDest=/LeonEdTest/Arm\nType=SkeletalMesh\n\n[ArmAnimations]\nSource=") + Arm +
			TEXT("\nDest=/LeonEdTest/Arm\nType=Animation\nSkeleton=/LeonEdTest/Arm/SKEL_SkinnedArm.SKEL_SkinnedArm\n\n"
				 "[BS_ArmMove]\nType=BlendSpace\nDest=/LeonEdTest/Arm\nAxisX=Speed,0,600\nAxisY=Direction,-180,180\n"
				 "+Sample=A_Wave,0,0\n+Sample=A_Grip,600,0\n+Sample=/LeonEdTest/Arm/A_Wave,300,180\n\n"
				 "[BS_ArmSpeed]\nType=BlendSpace1D\nDest=/LeonEdTest/Arm\nAxisX=Speed,0,600\n+Sample=A_Wave,0\n"
				 "+Sample=A_Grip,600\n\n"
				 "[AO_ArmAim]\nType=AimOffsetBlendSpace1D\nDest=/LeonEdTest/Arm\n+Sample=A_Wave,-90\n+Sample=A_Grip,0\n"
				 "+Sample=A_Wave,90\nBasePose=A_Grip\n\n"
				 "[AM_ArmWave]\nType=AnimMontage\nDest=/LeonEdTest/Arm\nAnimation=A_Wave\nSlotName=UpperBody\n"
				 "BlendInTime=0.2\nBlendOutTime=0.25\n+Section=Start,0\n+Section=Hold,0.5,Hold\n+Notify=MagIn,0.6\n"));
	TestEqual("-importlist", RunCommandlet(TEXT("ImportAssets"), TEXT("-importlist=\"") + List + TEXT("\"")), 0);
	const TCHAR* const Packages[] = {TEXT("/LeonEdTest/Arm/BS_ArmMove"), TEXT("/LeonEdTest/Arm/BS_ArmSpeed"),
		TEXT("/LeonEdTest/Arm/AO_ArmAim"), TEXT("/LeonEdTest/Arm/AM_ArmWave")};
	TArray<TArray<uint8>> First;
	for (const TCHAR* Package : Packages)
	{
		First.Add(LeonEdTest::ReadBytes(PackageFile(Package)));
		TestTrue(*FString::Printf(TEXT("%s saved"), Package), First.Last().Num() > 0);
	}
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);

	const UBlendSpace* Move = LoadObject<UBlendSpace>(nullptr, TEXT("/LeonEdTest/Arm/BS_ArmMove.BS_ArmMove"));
	const UAnimSequence* Wave = LoadObject<UAnimSequence>(nullptr, TEXT("/LeonEdTest/Arm/A_Wave.A_Wave"));
	if (!TestNotNull("BS_ArmMove loads", Move) || !TestNotNull("A_Wave loads", Wave))
	{
		return false;
	}
	TestTrue("Its axes",
		Move->GetBlendParameter(0).DisplayName == TEXT("Speed") && Move->GetBlendParameter(1).Min == -180.0f &&
			Move->GetBlendParameter(0).Max == 600.0f);
	TestTrue("Its samples, in order",
		Move->GetBlendSamples().Num() == 3 && Move->GetBlendSamples()[0].Animation == Wave &&
			Move->GetBlendSamples()[2].SampleValue.Equals(FVector(300.0f, 180.0f, 0.0f)));
	TestEqual("Triangulated when loaded", Move->GetTriangles().Num(), 1);
	TestTrue("On the clips' skeleton", Move->GetSkeleton() == Wave->GetSkeleton());
	TestTrue("The clip's notifies from its glTF extras", Wave->Notifies.Num() == 2);
	const UBlendSpace1D* Speed = LoadObject<UBlendSpace1D>(nullptr, TEXT("/LeonEdTest/Arm/BS_ArmSpeed.BS_ArmSpeed"));
	TestTrue("BS_ArmSpeed",
		Speed != nullptr && Speed->GetBlendSamples().Num() == 2 && Speed->GetBlendSamples()[1].SampleValue.X == 600.0f);
	const UAimOffsetBlendSpace1D* Aim =
		LoadObject<UAimOffsetBlendSpace1D>(nullptr, TEXT("/LeonEdTest/Arm/AO_ArmAim.AO_ArmAim"));
	TestTrue("AO_ArmAim: pitch -90..90, its base pose",
		Aim != nullptr && Aim->GetBlendSamples().Num() == 3 && Aim->GetBlendParameter(0).Min == -90.0f &&
			Aim->BasePose != nullptr && Aim->BasePose->GetName() == TEXT("A_Grip"));
	const UAnimMontage* Montage = LoadObject<UAnimMontage>(nullptr, TEXT("/LeonEdTest/Arm/AM_ArmWave.AM_ArmWave"));
	TestTrue("AM_ArmWave",
		Montage != nullptr && Montage->Animation == Wave && Montage->SlotName == FName(TEXT("UpperBody")) &&
			Montage->BlendInTime == 0.2f && Montage->CompositeSections.Num() == 2 &&
			Montage->CompositeSections[1].NextSectionName == FName(TEXT("Hold")) && Montage->Notifies.Num() == 1 &&
			Montage->GetPlayLength() == Wave->GetPlayLength());
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);

	TestEqual("-importlist again", RunCommandlet(TEXT("ImportAssets"), TEXT("-importlist=\"") + List + TEXT("\"")), 0);
	for (int32 Index = 0; Index < int32(UE_ARRAY_COUNT(Packages)); ++Index)
	{
		TestTrue(*FString::Printf(TEXT("%s: the same bytes"), Packages[Index]),
			LeonEdTest::ReadBytes(PackageFile(Packages[Index])) == First[Index]);
	}
	LeonEdTest::DestroyPackagesUnder(LeonEdTest::Root);

	// A sample naming a clip that does not exist fails the section.
	const FString Bad = LeonEdTest::WriteSource(
		TEXT("BadAnimationList.ini"), TEXT("[BS_Bad]\nType=BlendSpace1D\nDest=/LeonEdTest/Arm\n+Sample=A_Missing,0\n"));
	AddExpectedError(TEXT("is not a AnimSequence"), 1);
	AddExpectedError(TEXT("making /LeonEdTest/Arm/BS_Bad failed"), 1);
	TestEqual("A missing clip", RunCommandlet(TEXT("ImportAssets"), TEXT("-importlist=\"") + Bad + TEXT("\"")), 1);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

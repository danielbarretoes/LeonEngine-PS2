#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/Skeleton.h"
#include "CoreMinimal.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PhysicsEngine/BodySetup.h"
#include "Primitives.h"
#include "Sound/SoundWave.h"
#include "Tests/EngineTestTypes.h"
#include "UObject/GarbageCollection.h"
#include "UObject/LinkerLoad.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"

#if WITH_DEV_AUTOMATION_TESTS

// The asset classes saved to `.lasset` packages and loaded back (SavePackage / LoadPackage): every class keeps its
// tagged properties, its native data and its bulk data, and references between assets of different packages resolve
// once the packages are loaded again. The packages are saved to memory under a test mount point, destroyed (as if the
// process had restarted) and loaded back.

namespace
{

	/** The tests' mount point; nothing is written to its folder (the packages are saved to memory). */
	const TCHAR* const AssetTestRoot = TEXT("/AssetTest/");

	FString GetAssetTestContentDir()
	{
		return FPaths::ProjectIntermediateDir() + TEXT("Tests/EngineAssets/");
	}

	/** Marks a package and everything in it pending kill and collects it. */
	void DestroyPackage(const FString& PackageName)
	{
		if (UPackage* Package = FindPackage(nullptr, *PackageName))
		{
			TArray<UObject*> Objects;
			GetObjectsWithOuter(Package, Objects, /*bIncludeNestedObjects =*/true);
			for (UObject* Object : Objects)
			{
				Object->MarkPendingKill();
			}
			Package->MarkPendingKill();
		}
	}

	/**
	 * Registers the test mount point for a test's lifetime; the packages it saved are unregistered and destroyed at the
	 * end.
	 */
	class FAssetTestScope
	{
	public:
		FAssetTestScope()
		{
			FPackageName::RegisterMountPoint(AssetTestRoot, GetAssetTestContentDir());
		}

		~FAssetTestScope()
		{
			DestroyAll();
			for (const FString& PackageName : PackageNames)
			{
				FLinkerLoad::UnregisterInMemoryPackage(PackageName);
			}
			FPackageName::UnRegisterMountPoint(AssetTestRoot, GetAssetTestContentDir());
		}

		FAssetTestScope(const FAssetTestScope&) = delete;
		FAssetTestScope& operator=(const FAssetTestScope&) = delete;

		/** A new package under the test root. */
		UPackage* NewPackage(const TCHAR* ShortName)
		{
			const FString PackageName = FString(AssetTestRoot) + ShortName;
			PackageNames.AddUnique(PackageName);
			return CreatePackage(*PackageName);
		}

		/** Saves the package's public objects to memory and registers the bytes under its name. */
		bool Save(FAutomationTestBase& Test, UPackage* Package, TArray<uint8>* OutBytes = nullptr)
		{
			TArray<uint8> Bytes;
			const FSavePackageResultStruct Result = UPackage::SaveToMemory(Package, nullptr, RF_Public, Bytes);
			if (!Test.TestTrue(*FString::Printf(TEXT("%s saves"), *Package->GetName()), Result.IsSuccessful()))
			{
				return false;
			}
			FLinkerLoad::RegisterInMemoryPackage(Package->GetName(), Bytes);
			if (OutBytes != nullptr)
			{
				*OutBytes = MoveTemp(Bytes);
			}
			return true;
		}

		/** Destroys every package of the test, as a new process would start without them. */
		void DestroyAll()
		{
			for (const FString& PackageName : PackageNames)
			{
				DestroyPackage(PackageName);
			}
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS, /*bPerformFullPurge =*/true);
		}

		TArray<FString> PackageNames;
	};

	constexpr EObjectFlags AssetFlags = RF_Public | RF_Standalone;

	/** SizeX x SizeY texels whose bytes count up from Seed. */
	TArray<uint8> MakeTexels(int32 SizeX, int32 SizeY, uint8 Seed)
	{
		TArray<uint8> Texels;
		Texels.SetNumUninitialized(SizeX * SizeY * 4);
		for (int32 Index = 0; Index < Texels.Num(); ++Index)
		{
			Texels[Index] = static_cast<uint8>(Seed + Index);
		}
		return Texels;
	}

	/** Mip 0's texels. */
	TArray<uint8> ReadTexels(const UTexture2D& Texture)
	{
		TArray<uint8> Texels;
		if (Texture.GetNumMips() == 0)
		{
			return Texels;
		}
		const FByteBulkData& BulkData = Texture.GetPlatformData().Mips[0].BulkData;
		Texels.SetNumUninitialized(static_cast<int32>(BulkData.GetBulkDataSize()));
		if (Texels.Num() > 0)
		{
			FMemory::Memcpy(Texels.GetData(), BulkData.LockReadOnly(), Texels.Num());
			BulkData.Unlock();
		}
		return Texels;
	}

	/** Two sections (the cube's first and last faces) drawn with slots 1 and 0. */
	FMeshData MakeTwoSectionCube()
	{
		FMeshData Data = MakeCube();
		Data.Submeshes.Add(FMeshSection{0, 6, 1});
		Data.Submeshes.Add(FMeshSection{6, Data.Indices.Num() - 6, 0});
		return Data;
	}

	bool SameVertices(const TArray<FVertex>& A, const TArray<FVertex>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 Index = 0; Index < A.Num(); ++Index)
		{
			if (A[Index].Position != B[Index].Position || A[Index].Normal != B[Index].Normal ||
				A[Index].TexCoord != B[Index].TexCoord || A[Index].Tangent != B[Index].Tangent)
			{
				return false;
			}
		}
		return true;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetTexture2DRoundTripTest, "System.Engine.Assets.Texture2DRoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAssetTexture2DRoundTripTest::RunTest(const FString& Parameters)
{
	// A texture keeps its size, format, sRGB flag and texels; the texels are bulk data at the end of the file.
	FAssetTestScope Scope;
	const TArray<uint8> Texels = MakeTexels(4, 2, 7);
	{
		UPackage* Package = Scope.NewPackage(TEXT("T_Test"));
		UTexture2D* Texture = NewObject<UTexture2D>(Package, TEXT("T_Test"), AssetFlags);
		TestTrue("Texels set", Texture->SetPlatformData(4, 2, PF_R8G8B8A8, Texels.GetData()));
		TestFalse("An empty size is refused", Texture->SetPlatformData(0, 2, PF_R8G8B8A8, Texels.GetData()));
		Texture->SRGB = 0;
		TArray<uint8> Bytes;
		if (!Scope.Save(*this, Package, &Bytes))
		{
			return false;
		}
		TestTrue("The texels are in the package", Bytes.Num() > Texels.Num());
	}
	Scope.DestroyAll();
	TestNull("Destroyed", FindObject<UTexture2D>(nullptr, TEXT("/AssetTest/T_Test.T_Test")));

	const UTexture2D* Loaded = LoadObject<UTexture2D>(nullptr, TEXT("/AssetTest/T_Test.T_Test"));
	if (!TestNotNull("Loaded", Loaded))
	{
		return false;
	}
	TestEqual("SizeX", Loaded->GetSizeX(), 4);
	TestEqual("SizeY", Loaded->GetSizeY(), 2);
	TestTrue("Format", Loaded->GetPixelFormat() == PF_R8G8B8A8);
	TestEqual("One mip", Loaded->GetNumMips(), 1);
	TestTrue("Valid", Loaded->HasValidPlatformData());
	TestEqual("Not sRGB", static_cast<int32>(Loaded->SRGB), 0);
	TestTrue("Texels", ReadTexels(*Loaded) == Texels);
	TestTrue("The texels were bulk data at the end of the file",
		Loaded->GetPlatformData().Mips[0].BulkData.GetBulkDataOffsetInFile() >= 0);

	UTexture2D* Transient = UTexture2D::CreateTransient(8, 8);
	if (TestNotNull("CreateTransient", Transient))
	{
		TestTrue("Transient texture in the transient package", Transient->GetOutermost() == GetTransientPackage());
		TestEqual("Transient texels zeroed", static_cast<int32>(ReadTexels(*Transient)[5]), 0);
	}
	TestNull("CreateTransient refuses an empty size", UTexture2D::CreateTransient(0, 8));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetStaticMeshRoundTripTest, "System.Engine.Assets.StaticMeshRoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAssetStaticMeshRoundTripTest::RunTest(const FString& Parameters)
{
	// A mesh keeps its geometry (bulk data), bounds, sections and body setup (an inner object), and its slots'
	// materials, in another package, come back with their parameters and their own texture from a third package.
	FAssetTestScope Scope;
	const FMeshData Data = MakeTwoSectionCube();
	const TArray<uint8> Texels = MakeTexels(2, 2, 40);
	FMaterial SavedValues;
	FBox SavedBounds;
	{
		UPackage* TexturePackage = Scope.NewPackage(TEXT("T_Grid"));
		UTexture2D* Texture = NewObject<UTexture2D>(TexturePackage, TEXT("T_Grid"), AssetFlags);
		(void)Texture->SetPlatformData(2, 2, PF_R8G8B8A8, Texels.GetData());

		UPackage* MaterialPackage = Scope.NewPackage(TEXT("M_Grid"));
		UMaterial* Material = NewObject<UMaterial>(MaterialPackage, TEXT("M_Grid"), AssetFlags);
		Material->ShadingModel = MSM_Unlit;
		Material->BaseColor = FLinearColor(0.25f, 0.5f, 0.75f, 1.0f);
		Material->Specular = FLinearColor(0.1f, 0.2f, 0.3f, 1.0f);
		Material->Metallic = 0.5f;
		Material->Roughness = 0.3f;
		Material->Opacity = 0.75f;
		Material->Shininess = 64.0f;
		Material->UVScale = FVector2D(8.0f, 4.0f);
		Material->bCastsShadows = false;
		Material->bPlanarMirror = true;
		Material->BaseColorMap = Texture;
		SavedValues = Material->GetRenderProxy();

		UPackage* MeshPackage = Scope.NewPackage(TEXT("SM_Cube"));
		UStaticMesh* Mesh = NewObject<UStaticMesh>(MeshPackage, TEXT("SM_Cube"), AssetFlags);
		TestTrue("Built", Mesh->BuildFromMeshData(Data));
		TestFalse("Empty data is refused", Mesh->BuildFromMeshData(FMeshData()));
		Mesh->StaticMaterials.Add(FStaticMaterial(nullptr, TEXT("Unused")));
		Mesh->StaticMaterials.Add(FStaticMaterial(Material, TEXT("Grid")));
		if (!TestNotNull("A body setup", Mesh->GetBodySetup()))
		{
			return false;
		}
		Mesh->GetBodySetup()->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		Mesh->GetBodySetup()->AggGeom.BoxElems.Add(FKBoxElem(100.0f, 50.0f, 25.0f));
		SavedBounds = Mesh->GetBoundingBox();

		if (!Scope.Save(*this, TexturePackage) || !Scope.Save(*this, MaterialPackage) ||
			!Scope.Save(*this, MeshPackage))
		{
			return false;
		}
	}
	Scope.DestroyAll();

	// Loading the mesh loads the material's and the texture's packages (its imports).
	const UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/AssetTest/SM_Cube.SM_Cube"));
	if (!TestNotNull("Mesh loaded", Mesh))
	{
		return false;
	}
	const FStaticMeshLODResources& LOD = Mesh->GetLODResources();
	TestTrue("Vertices", SameVertices(LOD.Vertices, Data.Vertices));
	TestTrue("Indices", LOD.Indices == Data.Indices);
	TestEqual("Sections", Mesh->GetNumSections(), 2);
	TestEqual("Section 0 slot", LOD.Sections[0].MaterialIndex, 1);
	TestEqual("Section 1 range", LOD.Sections[1].IndexCount, Data.Indices.Num() - 6);
	TestTrue("Bounds", Mesh->GetBoundingBox().Min == SavedBounds.Min && Mesh->GetBoundingBox().Max == SavedBounds.Max);
	TestEqual("Triangles", Mesh->GetNumTriangles(), Data.Indices.Num() / 3);

	const UBodySetup* BodySetup = Mesh->GetBodySetup();
	if (TestNotNull("Body setup loaded", BodySetup))
	{
		TestTrue("The body setup is the mesh's inner object", BodySetup->GetOuter() == Mesh);
		TestTrue("Trace flag", BodySetup->GetCollisionTraceFlag() == CTF_UseSimpleAsComplex);
		TestEqual("One box", BodySetup->AggGeom.BoxElems.Num(), 1);
		TestFalse("Simple collision everywhere", BodySetup->UsesComplexAsSimpleForStaticBodies());
	}

	TestEqual("Slots", Mesh->GetStaticMaterials().Num(), 2);
	TestNull("An empty slot", Mesh->GetMaterial(0));
	TestTrue("Slot name", Mesh->GetStaticMaterials()[1].MaterialSlotName == FName(TEXT("Grid")));
	const UMaterial* Material = Cast<UMaterial>(Mesh->GetMaterial(1));
	if (!TestNotNull("The slot's material resolved", Material))
	{
		return false;
	}
	TestTrue("The material came from its own package", Material->GetOutermost()->GetName() == "/AssetTest/M_Grid");
	const FMaterial Values = Material->GetRenderProxy();
	TestTrue("Unlit", Values.Shading == EMaterialLightingModel::Unlit);
	TestTrue("Base colour", Values.Albedo == SavedValues.Albedo);
	TestTrue("Specular", Values.Specular == SavedValues.Specular);
	TestEqual("Metallic", Values.Metallic, SavedValues.Metallic);
	TestEqual("Roughness", Values.Roughness, SavedValues.Roughness);
	TestEqual("Opacity", Values.Alpha, SavedValues.Alpha);
	TestEqual("Shininess", Values.Shininess, SavedValues.Shininess);
	TestTrue("UV scale", Values.UvScale == SavedValues.UvScale);
	TestFalse("Casts no shadows", Values.bCastsShadows);
	TestTrue("Mirror", Values.bPlanarMirror);
	TestNull("No normal map", Values.NormalMap);
	if (TestNotNull("The base colour map resolved", Values.AlbedoMap))
	{
		TestTrue("Its texels", ReadTexels(*Values.AlbedoMap) == Texels);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetMaterialDefaultsTest, "System.Engine.Assets.MaterialDefaults",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAssetMaterialDefaultsTest::RunTest(const FString& Parameters)
{
	// A new material draws what an unset slot always drew (the renderer's default FMaterial), and a renderer FMaterial
	// round-trips through a material (the `.lmat` path).
	const FMaterial Default;
	const UMaterial& Material = *NewObject<UMaterial>();
	const FMaterial Values = Material.GetRenderProxy();
	TestTrue("Lit", Values.Shading == Default.Shading);
	TestTrue("Albedo", Values.Albedo == Default.Albedo);
	TestTrue("Specular", Values.Specular == Default.Specular);
	TestEqual("Metallic", Values.Metallic, Default.Metallic);
	TestEqual("Alpha", Values.Alpha, Default.Alpha);
	TestEqual("Shininess", Values.Shininess, Default.Shininess);
	TestEqual("Roughness", Values.Roughness, Default.Roughness);
	TestTrue("UV scale", Values.UvScale == Default.UvScale);
	TestTrue("Shadows", Values.bCastsShadows == Default.bCastsShadows);
	TestTrue("Mirror", Values.bPlanarMirror == Default.bPlanarMirror);

	FMaterial Custom;
	Custom.Shading = EMaterialLightingModel::Unlit;
	Custom.Albedo = FVector(0.1f, 0.2f, 0.3f);
	Custom.Alpha = 0.5f;
	Custom.Roughness = 0.9f;
	UMaterial& Copy = *NewObject<UMaterial>();
	Copy.SetFromRenderProxy(Custom);
	const FMaterial Back = Copy.GetRenderProxy();
	TestTrue("Unlit back", Back.Shading == EMaterialLightingModel::Unlit);
	TestTrue("Albedo back", Back.Albedo == Custom.Albedo);
	TestEqual("Alpha back", Back.Alpha, 0.5f);
	TestEqual("Roughness back", Back.Roughness, 0.9f);
	TestTrue("Translucent", Copy.IsTranslucent());
	TArray<UTexture*> Used;
	Copy.GetUsedTextures(Used);
	TestEqual("No textures", Used.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetSkeletalRoundTripTest, "System.Engine.Assets.SkeletalRoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAssetSkeletalRoundTripTest::RunTest(const FString& Parameters)
{
	// A skeleton (bones and a socket), a skinned mesh, a clip (tracks as bulk data) and a blend space, each in its own
	// package, come back with their data and their references to each other.
	FAssetTestScope Scope;
	FSkeletalMeshData Data;
	Data.RefSkeleton.BoneNames = {FName("root"), FName("hand")};
	Data.RefSkeleton.ParentIndices = {INDEX_NONE, 0};
	Data.RefSkeleton.InverseBindPose = {FMatrix::Identity, FTranslationMatrix(FVector(0.0f, -10.0f, 0.0f))};
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FSkeletalVertex Vertex;
		Vertex.Position = FVector(static_cast<float>(Index), 2.0f, 3.0f);
		Vertex.BoneIndices = FIntVector4(Index % 2, 0, 0, 0);
		Vertex.BoneWeights = FVector4(1.0f, 0.0f, 0.0f, 0.0f);
		Data.Vertices.Add(Vertex);
		Data.Indices.Add(static_cast<uint32>(Index));
	}
	Data.LocalMin = FVector(0.0f, 2.0f, 3.0f);
	Data.LocalMax = FVector(2.0f, 2.0f, 3.0f);
	FRawAnimSequence Raw;
	Raw.SequenceLength = 2.0f;
	Raw.FrameRate = 1.0f;
	Raw.bLoop = false;
	Raw.Tracks.SetNum(2);
	for (FRawAnimSequenceTrack& Track : Raw.Tracks)
	{
		Track.Keys = {FMatrix::Identity, FTranslationMatrix(FVector(4.0f, 0.0f, 0.0f))};
	}
	{
		UPackage* SkeletonPackage = Scope.NewPackage(TEXT("SKEL_Hand"));
		USkeleton* Skeleton = NewObject<USkeleton>(SkeletonPackage, TEXT("SKEL_Hand"), AssetFlags);
		Skeleton->SetReferenceSkeleton(Data.RefSkeleton);
		Skeleton->AddSocket(TEXT("Grip"), TEXT("hand"), FTransform(FVector(1.0f, 2.0f, 3.0f)));

		UPackage* MeshPackage = Scope.NewPackage(TEXT("SK_Hand"));
		USkeletalMesh* Mesh = NewObject<USkeletalMesh>(MeshPackage, TEXT("SK_Hand"), AssetFlags);
		TestTrue("Mesh built", Mesh->BuildFromImportData(Data, Skeleton));

		UPackage* ClipPackage = Scope.NewPackage(TEXT("A_Wave"));
		UAnimSequence* Clip = NewObject<UAnimSequence>(ClipPackage, TEXT("A_Wave"), AssetFlags);
		Clip->SetFromRawAnimSequence(Raw);
		Clip->SetSkeleton(Skeleton);

		UPackage* BlendPackage = Scope.NewPackage(TEXT("BS_Move"));
		UBlendSpace1D* BlendSpace = NewObject<UBlendSpace1D>(BlendPackage, TEXT("BS_Move"), AssetFlags);
		BlendSpace->BlendParameters[0].Max = 600.0f;
		BlendSpace->AddSample(Clip, 300.0f);
		BlendSpace->SetSkeleton(Skeleton);

		if (!Scope.Save(*this, SkeletonPackage) || !Scope.Save(*this, MeshPackage) || !Scope.Save(*this, ClipPackage) ||
			!Scope.Save(*this, BlendPackage))
		{
			return false;
		}
	}
	Scope.DestroyAll();

	const UBlendSpace1D* BlendSpace = LoadObject<UBlendSpace1D>(nullptr, TEXT("/AssetTest/BS_Move.BS_Move"));
	const USkeletalMesh* Mesh = LoadObject<USkeletalMesh>(nullptr, TEXT("/AssetTest/SK_Hand.SK_Hand"));
	if (!TestNotNull("Blend space loaded", BlendSpace) || !TestNotNull("Mesh loaded", Mesh))
	{
		return false;
	}
	const USkeleton* Skeleton = Mesh->Skeleton;
	if (!TestNotNull("The mesh's skeleton resolved", Skeleton))
	{
		return false;
	}
	TestEqual("Bones", Skeleton->GetReferenceSkeleton().GetNum(), 2);
	TestEqual("Bone name", Skeleton->GetReferenceSkeleton().FindBoneIndex(FName("hand")), 1);
	TestTrue("Inverse bind pose",
		Skeleton->GetReferenceSkeleton().InverseBindPose[1] == Data.RefSkeleton.InverseBindPose[1]);
	const USkeletalMeshSocket* Socket = Skeleton->FindSocket(TEXT("Grip"));
	if (TestNotNull("Socket", Socket))
	{
		TestTrue("Socket bone", Socket->BoneName == FName(TEXT("hand")));
		TestTrue("Socket offset", Socket->RelativeLocation.Equals(FVector(1.0f, 2.0f, 3.0f)));
	}
	TestEqual("Skinned vertices", Mesh->GetVertices().Num(), 3);
	TestEqual("Bone index", Mesh->GetVertices()[1].BoneIndices.X, 1);
	TestTrue("Indices", Mesh->GetIndices() == Data.Indices);
	TestTrue("Bounds", Mesh->GetBoundingBox().Max == Data.LocalMax);
	TestTrue("The mesh's bones are the skeleton's", Mesh->GetRefSkeleton().GetNum() == 2);

	TestEqual("Axis", BlendSpace->GetBlendParameter(0).Max, 600.0f);
	TestTrue("The blend space's skeleton", BlendSpace->GetSkeleton() == Skeleton);
	if (!TestEqual("One sample", BlendSpace->GetBlendSamples().Num(), 1))
	{
		return false;
	}
	const UAnimSequence* Clip = BlendSpace->GetBlendSamples()[0].Animation;
	if (!TestNotNull("The sample's clip resolved", Clip))
	{
		return false;
	}
	TestEqual("Sample position", BlendSpace->GetBlendSamples()[0].SampleValue.X, 300.0f);
	TestEqual("Length", Clip->SequenceLength, 2.0f);
	TestEqual("Frames", Clip->GetNumberOfFrames(), 2);
	TestEqual("Tracks", Clip->GetNumberOfTracks(), 2);
	TestFalse("One-shot", Clip->bLoop);
	TArray<FMatrix> Pose;
	Clip->GetBonePose(0.5f, Pose);
	TestEqual("Sampled between the keys", Pose[1].M[3][0], 2.0f, 1.0e-4f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetSoundWaveRoundTripTest, "System.Engine.Assets.SoundWaveRoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAssetSoundWaveRoundTripTest::RunTest(const FString& Parameters)
{
	// A sound keeps its PCM16 samples (bulk data), channels, rate and duration.
	FAssetTestScope Scope;
	TArray<int16> Samples;
	for (int32 Index = 0; Index < 200; ++Index)
	{
		Samples.Add(static_cast<int16>((Index * 331) - 30000));
	}
	{
		UPackage* Package = Scope.NewPackage(TEXT("S_Beep"));
		USoundWave* Sound = NewObject<USoundWave>(Package, TEXT("S_Beep"), AssetFlags);
		TestFalse("No rate is refused", Sound->SetPCMData(Samples.GetData(), 100, 2, 0));
		TestTrue("Samples set", Sound->SetPCMData(Samples.GetData(), 100, 2, 22050));
		if (!Scope.Save(*this, Package))
		{
			return false;
		}
	}
	Scope.DestroyAll();

	const USoundWave* Sound = LoadObject<USoundWave>(nullptr, TEXT("/AssetTest/S_Beep.S_Beep"));
	if (!TestNotNull("Loaded", Sound))
	{
		return false;
	}
	TestEqual("Channels", Sound->NumChannels, 2);
	TestEqual("Rate", Sound->SampleRate, 22050);
	TestEqual("Frames", Sound->GetNumFrames(), 100);
	TestEqual("Duration", Sound->GetDuration(), 100.0f / 22050.0f, 1.0e-6f);
	TArray<int16> Loaded;
	Sound->GetPCMData(Loaded);
	TestTrue("Samples", Loaded == Samples);
	TestTrue(
		"The samples were bulk data at the end of the file", Sound->GetRawPCMData().GetBulkDataOffsetInFile() >= 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetDataAssetRoundTripTest, "System.Engine.Assets.DataAssetRoundTrip",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAssetDataAssetRoundTripTest::RunTest(const FString& Parameters)
{
	// A game's data asset keeps its properties and its reference to a texture in another package.
	FAssetTestScope Scope;
	{
		UPackage* TexturePackage = Scope.NewPackage(TEXT("T_Icon"));
		UTexture2D* Icon = NewObject<UTexture2D>(TexturePackage, TEXT("T_Icon"), AssetFlags);
		(void)Icon->SetPlatformData(1, 1, PF_R8G8B8A8, MakeTexels(1, 1, 3).GetData());

		UPackage* Package = Scope.NewPackage(TEXT("DA_Weapon"));
		UEngineTestDataAsset* Asset = NewObject<UEngineTestDataAsset>(Package, TEXT("DA_Weapon"), AssetFlags);
		Asset->Count = 30;
		Asset->Label = TEXT("Rifle");
		Asset->Tags = {FName("Primary"), FName("Auto")};
		Asset->Icon = Icon;
		if (!Scope.Save(*this, TexturePackage) || !Scope.Save(*this, Package))
		{
			return false;
		}
	}
	Scope.DestroyAll();

	const UEngineTestDataAsset* Asset =
		LoadObject<UEngineTestDataAsset>(nullptr, TEXT("/AssetTest/DA_Weapon.DA_Weapon"));
	if (!TestNotNull("Loaded", Asset))
	{
		return false;
	}
	TestTrue("A data asset", Asset->IsA<UDataAsset>());
	TestEqual("Count", Asset->Count, 30);
	TestEqual("Label", Asset->Label, FString("Rifle"));
	TestEqual("Tags", Asset->Tags.Num(), 2);
	if (TestNotNull("Icon resolved", Asset->Icon))
	{
		TestEqual("Icon size", Asset->Icon->GetSizeX(), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAssetCommandletTest, "System.Engine.Assets.Commandlet",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FAssetCommandletTest::RunTest(const FString& Parameters)
{
	// A commandlet's command line splits into tokens, switches and switch values; Main returns the exit code.
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> Params;
	UCommandlet::ParseCommandLine(
		TEXT("Game/Maps -reimport -source=\"SourceArt/Cube.fbx\" -dest=/Game/Meshes All"), Tokens, Switches, Params);
	TestEqual("Tokens", Tokens.Num(), 2);
	TestEqual("First token", Tokens[0], FString("Game/Maps"));
	TestEqual("Switches", Switches.Num(), 3);
	TestTrue("Switch without value", Switches.Contains(TEXT("reimport")));
	TestTrue("Switch with value", Switches.Contains(TEXT("dest")));
	const FString* Dest = Params.Find(TEXT("dest"));
	TestTrue("Value", Dest != nullptr && *Dest == TEXT("/Game/Meshes"));
	const FString* Source = Params.Find(TEXT("source"));
	TestTrue("Quoted value", Source != nullptr && *Source == TEXT("SourceArt/Cube.fbx"));

	UEngineTestCommandlet& Commandlet = *NewObject<UEngineTestCommandlet>();
	TestTrue("Transient class", Commandlet.GetClass()->HasAnyClassFlags(CLASS_Transient));
	TestTrue("Commandlet defaults", Commandlet.IsClient && Commandlet.IsServer && Commandlet.ShowErrorCount);
	TestEqual("Main's exit code", Commandlet.Main(TEXT("A B -switch")), 2);
	TestEqual("Main's parameters", Commandlet.LastParams, FString("A B -switch"));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

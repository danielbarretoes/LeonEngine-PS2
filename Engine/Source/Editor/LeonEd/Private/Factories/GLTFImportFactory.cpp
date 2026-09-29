#include "Factories/GLTFImportFactory.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetImportUtils.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Factories/StaticMeshImport.h"
#include "GltfImport.h"
#include "LeonEdLog.h"
#include "MeshData.h"
#include "Misc/PackageName.h"
#include "StaticMeshBuilder.h"
#include "UObject/Package.h"

namespace
{

	/** The package of an asset named AssetName in the folder of the package of InParent. */
	FString SiblingPackageName(const UObject* InParent, const FString& AssetName)
	{
		return FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName()) + TEXT("/") + AssetName;
	}

	/**
	 * Adds the file's sockets to the skeleton, or updates those it has with their names (a skeleton shared by several
	 * meshes keeps the sockets of the others).
	 */
	void ApplySockets(USkeleton& Skeleton, const TArray<FBoneSocketData>& Sockets)
	{
		for (const FBoneSocketData& Source : Sockets)
		{
			const FName SocketName(*FAssetImportUtils::SanitizeName(Source.Name));
			USkeletalMeshSocket* Socket = Skeleton.AddSocket(SocketName, Source.BoneName, Source.RelativeTransform);
			Socket->BoneName = Source.BoneName;
			Socket->RelativeLocation = Source.RelativeTransform.GetLocation();
			Socket->RelativeRotation = Source.RelativeTransform.GetRotation().Rotator();
			Socket->RelativeScale = Source.RelativeTransform.GetScale3D();
		}
	}

} // namespace

UGLTFImportFactory::UGLTFImportFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UStaticMesh::StaticClass();
	Formats.Add(TEXT("gltf;GL Transmission Format"));
	Formats.Add(TEXT("glb;GL Transmission Format (binary)"));
	bEditorImport = 1;
}

UClass* UGLTFImportFactory::ResolveSupportedClass()
{
	switch (ImportType)
	{
		case EGLTFImportType::SkeletalMesh:
			return USkeletalMesh::StaticClass();
		case EGLTFImportType::Animation:
			return UAnimSequence::StaticClass();
		case EGLTFImportType::StaticMesh:
		default:
			return UStaticMesh::StaticClass();
	}
}

bool UGLTFImportFactory::DoesSupportClass(UClass* Class)
{
	return Class != nullptr &&
		(Class->IsChildOf(UStaticMesh::StaticClass()) || Class->IsChildOf(USkeletalMesh::StaticClass()) ||
			Class->IsChildOf(UAnimSequence::StaticClass()));
}

UObject* UGLTFImportFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, bool& bOutOperationCanceled)
{
	(void)Parms;
	bOutOperationCanceled = false;
	AdditionalImportedObjects.Reset();
	// The class asked for decides (a reimport asks for the asset's class), else ImportType.
	UClass* Class = InClass != nullptr && DoesSupportClass(InClass) ? InClass : ResolveSupportedClass();
	if (Class->IsChildOf(USkeletalMesh::StaticClass()))
	{
		return ImportSkeletalMesh(InParent, InName, Flags, Filename);
	}
	if (Class->IsChildOf(UAnimSequence::StaticClass()))
	{
		return ImportAnimations(InParent, InName, Flags, Filename);
	}
	return ImportStaticMesh(InParent, InName, Flags, Filename);
}

bool UGLTFImportFactory::ParseLODs(
	const FString& InLODs, TArray<FStaticMeshSourceModel>& OutSourceModels, FString& OutError)
{
	OutSourceModels.Reset();
	TArray<FString> Entries;
	InLODs.ParseIntoArray(Entries, TEXT(","), true);
	if (Entries.Num() == 0)
	{
		return true;
	}
	// LOD 0: the mesh itself, from a screen size of 1.
	OutSourceModels.AddDefaulted();
	for (const FString& Entry : Entries)
	{
		FString Share;
		FString Size;
		if (!Entry.TrimStartAndEnd().Split(TEXT("@"), &Share, &Size))
		{
			OutError = FString::Printf("'%s' is not <PercentTriangles>@<ScreenSize>", *Entry);
			return false;
		}
		FStaticMeshSourceModel& Model = OutSourceModels.AddDefaulted_GetRef();
		Model.ReductionSettings.PercentTriangles = FCString::Atof(*Share.TrimStartAndEnd());
		Model.ScreenSize = FCString::Atof(*Size.TrimStartAndEnd());
		const FStaticMeshSourceModel& Previous = OutSourceModels[OutSourceModels.Num() - 2];
		if (Model.ReductionSettings.PercentTriangles <= 0.0f || Model.ReductionSettings.PercentTriangles > 1.0f ||
			Model.ScreenSize <= 0.0f || Model.ScreenSize >= Previous.ScreenSize)
		{
			OutError =
				FString::Printf("'%s': the share must be in (0, 1] and each screen size below the one before", *Entry);
			return false;
		}
	}
	return true;
}

UObject* UGLTFImportFactory::ImportStaticMesh(
	UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename)
{
	FMeshData Data;
	FString Error;
	if (!FStaticMeshBuilder::BuildFromGltf(Filename, Data, Error))
	{
		UE_LOG(LogLeonEd, Error, "GLTFImportFactory: %s", *Error);
		return nullptr;
	}
	TArray<FStaticMeshSourceModel> SourceModels;
	if (!ParseLODs(LODs, SourceModels, Error))
	{
		UE_LOG(LogLeonEd, Error, "GLTFImportFactory: LODs '%s': %s", *LODs, *Error);
		return nullptr;
	}
	UStaticMesh* Mesh = CreateOrOverwriteAsset<UStaticMesh>(InParent, InName, Flags);
	if (Mesh == nullptr)
	{
		return nullptr;
	}
	// The LODs first: the build simplifies the source for each (N15).
	Mesh->SourceModels = MoveTemp(SourceModels);
	if (!StaticMeshImport::BuildStaticMesh(*Mesh, Data, bImportMaterials, AdditionalImportedObjects))
	{
		UE_LOG(LogLeonEd, Error, "GLTFImportFactory: '%s' makes no static mesh", *Filename);
		return nullptr;
	}
	UpdateAssetImportData(Mesh, Filename);
	return Mesh;
}

UObject* UGLTFImportFactory::ImportSkeletalMesh(
	UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename)
{
	FSkeletalMeshImportData Data;
	FString Error;
	if (!LoadSkeletalMeshFromGltf(Filename, Data, Error))
	{
		UE_LOG(LogLeonEd, Error, "GLTFImportFactory: %s", *Error);
		return nullptr;
	}
	USkeletalMesh* Existing = FindObjectFast<USkeletalMesh>(InParent, InName);
	USkeleton* UseSkeleton = Skeleton != nullptr ? Skeleton : (Existing != nullptr ? Existing->Skeleton : nullptr);
	if (UseSkeleton == nullptr)
	{
		// The skeleton next to the mesh: SK_Hero -> SKEL_Hero (or NewSkeletonName), made when it does not exist.
		FString BaseName = InName.ToString();
		BaseName.RemoveFromStart(TEXT("SK_"), ESearchCase::CaseSensitive);
		const FString SkeletonName = !NewSkeletonName.IsEmpty()
			? NewSkeletonName
			: FAssetImportUtils::MakeAssetName(USkeleton::StaticClass(), BaseName);
		const FString PackageName = SiblingPackageName(InParent, SkeletonName);
		UseSkeleton =
			Cast<USkeleton>(FAssetImportUtils::FindOrLoadAsset(USkeleton::StaticClass(), PackageName, SkeletonName));
		if (UseSkeleton == nullptr)
		{
			UseSkeleton =
				NewObject<USkeleton>(CreatePackage(*PackageName), FName(*SkeletonName), RF_Public | RF_Standalone);
			UseSkeleton->SetReferenceSkeleton(Data.RefSkeleton);
		}
	}
	if (!UseSkeleton->GetReferenceSkeleton().HasSameBones(Data.RefSkeleton))
	{
		UE_LOG(LogLeonEd, Error, "GLTFImportFactory: the bones of '%s' do not match %s (names, order and parents)",
			*Filename, *UseSkeleton->GetPathName());
		return nullptr;
	}
	// The skeleton next to the mesh takes the file's reference and bind poses (the same bones, the latest source); a
	// skeleton the import was given is kept as it is. Both take the file's sockets.
	if (Skeleton == nullptr)
	{
		UseSkeleton->SetReferenceSkeleton(Data.RefSkeleton);
	}
	ApplySockets(*UseSkeleton, Data.Sockets);
	AdditionalImportedObjects.Add(UseSkeleton);

	USkeletalMesh* Mesh = CreateOrOverwriteAsset<USkeletalMesh>(InParent, InName, Flags);
	if (Mesh == nullptr || !Mesh->BuildFromMeshData(Data.Mesh, Data.SkinWeights, UseSkeleton))
	{
		return nullptr;
	}

	// One slot per source slot; a slot keeps its material when its name did not change (UE), else a named one gets
	// M_<Name> next to the mesh.
	const TArray<FSkeletalMaterial> OldSlots = Mesh->Materials;
	Mesh->Materials.Reset();
	const FString MeshPackageName = Mesh->GetOutermost()->GetName();
	for (int32 Slot = 0; Slot < FMath::Max(Data.Mesh.Materials.Num(), 1); ++Slot)
	{
		const FString SourceName =
			Data.Mesh.MaterialSlotNames.IsValidIndex(Slot) ? Data.Mesh.MaterialSlotNames[Slot] : FString();
		const FName SlotName =
			SourceName.IsEmpty() ? FName(NAME_None) : FName(*FAssetImportUtils::SanitizeName(SourceName));
		UMaterialInterface* Material = nullptr;
		for (int32 OldIndex = 0; OldIndex < OldSlots.Num(); ++OldIndex)
		{
			if (OldSlots[OldIndex].MaterialSlotName == SlotName && (SlotName != NAME_None || OldIndex == Slot))
			{
				Material = OldSlots[OldIndex].MaterialInterface;
				break;
			}
		}
		if (Material == nullptr && bImportMaterials && SlotName != NAME_None)
		{
			Material = StaticMeshImport::FindOrCreateMaterial(
				SourceName, Data.Mesh, Slot, MeshPackageName, FString(), AdditionalImportedObjects);
		}
		StaticMeshImport::ApplyPhysicalMaterial(Material, SourceName, Data.Mesh, Slot, AdditionalImportedObjects);
		Mesh->Materials.Add(FSkeletalMaterial(Material, SlotName));
	}
	UpdateAssetImportData(Mesh, Filename);
	return Mesh;
}

UObject* UGLTFImportFactory::ImportAnimations(
	UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename)
{
	UAnimSequence* Existing = FindObjectFast<UAnimSequence>(InParent, InName);
	USkeleton* UseSkeleton = Skeleton != nullptr ? Skeleton : (Existing != nullptr ? Existing->GetSkeleton() : nullptr);
	if (UseSkeleton == nullptr)
	{
		UE_LOG(LogLeonEd, Error, "GLTFImportFactory: importing the animations of '%s' needs a Skeleton", *Filename);
		return nullptr;
	}
	TArray<FRawAnimSequence> Clips;
	FString Error;
	if (!LoadAnimSequencesFromGltf(Filename, UseSkeleton->GetReferenceSkeleton(), AnimationName, Clips, Error))
	{
		UE_LOG(LogLeonEd, Error, "GLTFImportFactory: %s", *Error);
		return nullptr;
	}

	UAnimSequence* First = nullptr;
	for (const FRawAnimSequence& Clip : Clips)
	{
		// One animation named by the settings is the asset asked for; every animation of the file is A_<Name>.
		UAnimSequence* Sequence = nullptr;
		if (!AnimationName.IsEmpty())
		{
			Sequence = CreateOrOverwriteAsset<UAnimSequence>(InParent, InName, Flags);
		}
		else
		{
			const FString AssetName =
				FAssetImportUtils::MakeAssetName(UAnimSequence::StaticClass(), Clip.Name.ToString());
			const FString PackageName = SiblingPackageName(InParent, AssetName);
			UPackage* Package = FindPackage(nullptr, *PackageName);
			if (Package == nullptr && FPackageName::DoesPackageExist(PackageName))
			{
				Package = LoadPackage(nullptr, *PackageName, LOAD_None);
			}
			if (Package == nullptr)
			{
				Package = CreatePackage(*PackageName);
			}
			Sequence = CreateOrOverwriteAsset<UAnimSequence>(Package, FName(*AssetName), Flags);
		}
		if (Sequence == nullptr)
		{
			return nullptr;
		}
		Sequence->SetSkeleton(UseSkeleton);
		if (!Sequence->SetFromRawAnimSequence(Clip))
		{
			UE_LOG(LogLeonEd, Error, "GLTFImportFactory: the animation '%s' of '%s' has no keys", *Clip.Name.ToString(),
				*Filename);
			return nullptr;
		}
		// The clip records its animation, so that its reimport takes that one.
		UAssetImportData* ImportData = UpdateAssetImportData(Sequence, Filename);
		if (ImportData != nullptr)
		{
			TMap<FString, FString> Settings = AppliedImportSettings;
			Settings.Add(TEXT("AnimationName"), Clip.Name.ToString());
			ImportData->SetImportSettings(Settings);
		}
		if (First == nullptr)
		{
			First = Sequence;
		}
		else
		{
			AdditionalImportedObjects.Add(Sequence);
		}
	}
	return First;
}

bool UGLTFImportFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	return FactoryCanReimport(Obj, OutFilenames);
}

void UGLTFImportFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	FactorySetReimportPaths(Obj, NewReimportPaths);
}

EReimportResult::Type UGLTFImportFactory::Reimport(UObject* Obj)
{
	return FactoryReimport(Obj);
}

void UGLTFImportFactory::GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const
{
	FactoryGetAdditionalReimportedObjects(OutObjects);
}

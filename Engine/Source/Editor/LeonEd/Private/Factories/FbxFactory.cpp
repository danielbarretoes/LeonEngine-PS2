#include "Factories/FbxFactory.h"

#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "AssetImportUtils.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#include "Factories/StaticMeshImport.h"
#include "FbxSkeletalImport.h"
#include "LeonEdLog.h"
#include "MeshData.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "StaticMeshBuilder.h"
#include "UObject/Package.h"

namespace
{

	/** True when two skeletons have the same bones in the same order. */
	bool HasSameBones(const FReferenceSkeleton& A, const FReferenceSkeleton& B)
	{
		if (A.GetNum() != B.GetNum())
		{
			return false;
		}
		for (int32 Bone = 0; Bone < A.GetNum(); ++Bone)
		{
			if (A.GetBoneName(Bone) != B.GetBoneName(Bone) || A.GetParentIndex(Bone) != B.GetParentIndex(Bone))
			{
				return false;
			}
		}
		return true;
	}

} // namespace

UFbxFactory::UFbxFactory(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	SupportedClass = UStaticMesh::StaticClass();
	Formats.Add(TEXT("fbx;FBX meshes and animations"));
	Formats.Add(TEXT("obj;OBJ static meshes"));
	bEditorImport = 1;
}

UClass* UFbxFactory::ResolveSupportedClass()
{
	switch (MeshTypeToImport)
	{
		case FBXIT_SkeletalMesh:
			return USkeletalMesh::StaticClass();
		case FBXIT_Animation:
			return UAnimSequence::StaticClass();
		default:
			return UStaticMesh::StaticClass();
	}
}

bool UFbxFactory::DoesSupportClass(UClass* Class)
{
	return Class != nullptr &&
		(Class->IsChildOf(UStaticMesh::StaticClass()) || Class->IsChildOf(USkeletalMesh::StaticClass()) ||
			Class->IsChildOf(UAnimSequence::StaticClass()));
}

UObject* UFbxFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	const FString& Filename, const TCHAR* Parms, bool& bOutOperationCanceled)
{
	(void)Parms;
	bOutOperationCanceled = false;
	AdditionalImportedObjects.Reset();
	// The class asked for decides (a reimport asks for the asset's class), else MeshTypeToImport.
	UClass* Class = InClass != nullptr && DoesSupportClass(InClass) ? InClass : ResolveSupportedClass();
	const bool bObj = FPaths::GetExtension(Filename) == TEXT("obj");
	if (Class->IsChildOf(USkeletalMesh::StaticClass()))
	{
		return bObj ? nullptr : ImportSkeletalMesh(InParent, InName, Flags, Filename);
	}
	if (Class->IsChildOf(UAnimSequence::StaticClass()))
	{
		return bObj ? nullptr : ImportAnimation(InParent, InName, Flags, Filename);
	}
	return ImportStaticMesh(InParent, InName, Flags, Filename);
}

UObject* UFbxFactory::ImportStaticMesh(UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename)
{
	FMeshData Data;
	FString Error;
	if (!FStaticMeshBuilder::BuildFromFile(Filename, Data, Error))
	{
		UE_LOG(LogLeonEd, Error, "FbxFactory: %s", *Error);
		return nullptr;
	}
	UStaticMesh* Mesh = CreateOrOverwriteAsset<UStaticMesh>(InParent, InName, Flags);
	if (Mesh == nullptr)
	{
		return nullptr;
	}
	StaticMeshImport::BuildStaticMesh(*Mesh, Data, bImportMaterials, AdditionalImportedObjects);
	UpdateAssetImportData(Mesh, Filename);
	return Mesh;
}

UObject* UFbxFactory::ImportSkeletalMesh(UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename)
{
	FSkeletalMeshData Data;
	if (!LoadSkeletalMeshFromFbx(Filename, Data) || Data.IsEmpty())
	{
		UE_LOG(LogLeonEd, Error, "FbxFactory: no skinned mesh in '%s'", *Filename);
		return nullptr;
	}
	USkeletalMesh* Existing = FindObjectFast<USkeletalMesh>(InParent, InName);
	USkeleton* UseSkeleton = Skeleton != nullptr ? Skeleton : (Existing != nullptr ? Existing->Skeleton : nullptr);
	if (UseSkeleton != nullptr)
	{
		if (!HasSameBones(UseSkeleton->GetReferenceSkeleton(), Data.RefSkeleton))
		{
			UE_LOG(LogLeonEd, Error, "FbxFactory: the bones of '%s' do not match %s", *Filename,
				*UseSkeleton->GetPathName());
			return nullptr;
		}
	}
	else
	{
		// A new skeleton next to the mesh: SK_Hero -> SKEL_Hero.
		FString BaseName = InName.ToString();
		BaseName.RemoveFromStart(TEXT("SK_"), ESearchCase::CaseSensitive);
		const FString SkeletonName = FAssetImportUtils::MakeAssetName(USkeleton::StaticClass(), BaseName);
		const FString PackageName =
			FPackageName::GetLongPackagePath(InParent->GetOutermost()->GetName()) + TEXT("/") + SkeletonName;
		UseSkeleton =
			Cast<USkeleton>(FAssetImportUtils::FindOrLoadAsset(USkeleton::StaticClass(), PackageName, SkeletonName));
		if (UseSkeleton == nullptr)
		{
			UseSkeleton =
				NewObject<USkeleton>(CreatePackage(*PackageName), FName(*SkeletonName), RF_Public | RF_Standalone);
			UseSkeleton->SetReferenceSkeleton(Data.RefSkeleton);
			AdditionalImportedObjects.Add(UseSkeleton);
		}
		else if (!HasSameBones(UseSkeleton->GetReferenceSkeleton(), Data.RefSkeleton))
		{
			UE_LOG(LogLeonEd, Error, "FbxFactory: the bones of '%s' do not match %s", *Filename,
				*UseSkeleton->GetPathName());
			return nullptr;
		}
	}
	USkeletalMesh* Mesh = CreateOrOverwriteAsset<USkeletalMesh>(InParent, InName, Flags);
	if (Mesh == nullptr || !Mesh->BuildFromImportData(Data, UseSkeleton))
	{
		return nullptr;
	}
	UpdateAssetImportData(Mesh, Filename);
	return Mesh;
}

UObject* UFbxFactory::ImportAnimation(UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename)
{
	UAnimSequence* Existing = FindObjectFast<UAnimSequence>(InParent, InName);
	USkeleton* UseSkeleton = Skeleton != nullptr ? Skeleton : (Existing != nullptr ? Existing->GetSkeleton() : nullptr);
	if (UseSkeleton == nullptr)
	{
		UE_LOG(LogLeonEd, Error, "FbxFactory: importing the animation '%s' needs a Skeleton", *Filename);
		return nullptr;
	}
	FRawAnimSequence Raw;
	if (!LoadAnimSequenceFromFbx(Filename, UseSkeleton->GetReferenceSkeleton(), Raw))
	{
		UE_LOG(LogLeonEd, Error, "FbxFactory: no animation in '%s'", *Filename);
		return nullptr;
	}
	UAnimSequence* Sequence = CreateOrOverwriteAsset<UAnimSequence>(InParent, InName, Flags);
	if (Sequence == nullptr)
	{
		return nullptr;
	}
	Sequence->SetSkeleton(UseSkeleton);
	Sequence->SetFromRawAnimSequence(Raw);
	UpdateAssetImportData(Sequence, Filename);
	return Sequence;
}

bool UFbxFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	return FactoryCanReimport(Obj, OutFilenames);
}

void UFbxFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	FactorySetReimportPaths(Obj, NewReimportPaths);
}

EReimportResult::Type UFbxFactory::Reimport(UObject* Obj)
{
	return FactoryReimport(Obj);
}

void UFbxFactory::GetAdditionalReimportedObjects(TArray<UObject*>& OutObjects) const
{
	FactoryGetAdditionalReimportedObjects(OutObjects);
}

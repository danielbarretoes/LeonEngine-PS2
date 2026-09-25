#include "Engine/SkeletalMesh.h"

#include "Animation/Skeleton.h"
#include "AssetBulkData.h"
#include "EngineLogs.h"

USkeletalMesh::USkeletalMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USkeletalMesh::BuildFromImportData(const FSkeletalMeshData& Data, USkeleton* InSkeleton)
{
	if (Data.IsEmpty())
	{
		return false;
	}
	Skeleton = InSkeleton;
	Vertices = Data.Vertices;
	Indices = Data.Indices;
	BoundingBox = FBox(Data.LocalMin, Data.LocalMax);
	return true;
}

const FReferenceSkeleton& USkeletalMesh::GetRefSkeleton() const
{
	static const FReferenceSkeleton EmptySkeleton;
	return Skeleton != nullptr ? Skeleton->GetReferenceSkeleton() : EmptySkeleton;
}

float USkeletalMesh::FitUniformScale(float FitHeight) const
{
	if (FitHeight <= 0.0f)
	{
		return 1.0f;
	}
	/** 0.1 cm: keeps a flat mesh from dividing by zero. The height is the Z extent. */
	const float Height = FMath::Max((BoundingBox.Max - BoundingBox.Min).Z, 0.1f);
	return FitHeight / Height;
}

UMaterialInterface* USkeletalMesh::GetMaterial(int32 MaterialIndex) const
{
	return Materials.IsValidIndex(MaterialIndex) ? Materials[MaterialIndex].MaterialInterface : nullptr;
}

void USkeletalMesh::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << BoundingBox;
	if (!SerializeBulkPayload(
			Ar, this, GeometryBulkData, [this](FArchive& PayloadAr) { PayloadAr << Vertices << Indices; }))
	{
		UE_LOG(LogEngine, Error, "USkeletalMesh %s: damaged geometry", *GetPathName());
		Vertices.Empty();
		Indices.Empty();
	}
}

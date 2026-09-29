#include "Engine/SkeletalMesh.h"

#include "Animation/Skeleton.h"
#include "AssetBulkData.h"
#include "EngineLogs.h"
#include "HAL/LowLevelMemTracker.h"
#include "IMeshBuilderModule.h"
#include "MeshData.h"
#include "RendererInterface.h"

USkeletalMesh::USkeletalMesh(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool USkeletalMesh::BuildFromMeshData(
	const FMeshData& Mesh, const TArray<FSkinWeightInfo>& SkinWeights, USkeleton* InSkeleton)
{
	if (Mesh.IsEmpty() || InSkeleton == nullptr || SkinWeights.Num() != Mesh.Vertices.Num())
	{
		UE_LOG(LogEngine, Error, "USkeletalMesh %s: no geometry, skeleton or weights to build from", *GetPathName());
		return false;
	}
	const FReferenceSkeleton& Bones = InSkeleton->GetReferenceSkeleton();
	for (const FSkinWeightInfo& Skin : SkinWeights)
	{
		if (int32(Skin.InfluenceBones[0]) >= Bones.GetNum() || int32(Skin.InfluenceBones[1]) >= Bones.GetNum())
		{
			UE_LOG(LogEngine, Error, "USkeletalMesh %s: a vertex names a bone %s does not have", *GetPathName(),
				*InSkeleton->GetPathName());
			return false;
		}
	}
	IMeshBuilderModule* Builder = IMeshBuilderModule::GetForRunningPlatform();
	if (Builder == nullptr)
	{
		UE_LOG(LogEngine, Error, "USkeletalMesh %s: no mesh builder in this program (MeshUtilities)", *GetPathName());
		return false;
	}
	FLPS2Mesh Built;
	FString Error;
	if (!Builder->BuildSkinnedMesh(Mesh, SkinWeights, Built, Error))
	{
		UE_LOG(LogEngine, Error, "USkeletalMesh %s: %s", *GetPathName(), *Error);
		return false;
	}
	Skeleton = InSkeleton;
	RenderData = MoveTemp(Built);

	// The bind pose's box, and how far each bone's vertices are from it in its bind space.
	FVector Min(TNumericLimits<float>::Max());
	FVector Max(TNumericLimits<float>::Lowest());
	BoneBoundsRadii.Init(0.0f, Bones.GetNum());
	for (int32 Index = 0; Index < Mesh.Vertices.Num(); ++Index)
	{
		const FVector& Position = Mesh.Vertices[Index].Position;
		Min = Min.ComponentMin(Position);
		Max = Max.ComponentMax(Position);
		for (int32 Influence = 0; Influence < MaxBoneInfluences; ++Influence)
		{
			const int32 Bone = SkinWeights[Index].InfluenceBones[Influence];
			if (SkinWeights[Index].InfluenceWeights[Influence] == 0 || !Bones.InverseBindPose.IsValidIndex(Bone))
			{
				continue;
			}
			const FVector InBone(Bones.InverseBindPose[Bone].TransformPosition(Position));
			BoneBoundsRadii[Bone] = FMath::Max(BoneBoundsRadii[Bone], InBone.Size());
		}
	}
	BoundingBox = FBox(Min, Max);
	InitResources();
	return true;
}

FBox USkeletalMesh::GetPoseBounds(const TArray<FMatrix>& ComponentSpaceTransforms) const
{
	if (ComponentSpaceTransforms.Num() != BoneBoundsRadii.Num())
	{
		return BoundingBox;
	}
	FBox Bounds(ForceInit);
	for (int32 Bone = 0; Bone < BoneBoundsRadii.Num(); ++Bone)
	{
		if (BoneBoundsRadii[Bone] <= 0.0f)
		{
			continue;
		}
		const FMatrix& Pose = ComponentSpaceTransforms[Bone];
		const float Radius = BoneBoundsRadii[Bone] * Pose.GetMaximumAxisScale();
		const FVector Origin = Pose.GetOrigin();
		Bounds += FBox(Origin - FVector(Radius), Origin + FVector(Radius));
	}
	return Bounds.IsValid ? Bounds : BoundingBox;
}

void USkeletalMesh::InitResources()
{
	ReleaseResources();
}

void USkeletalMesh::ReleaseResources()
{
	ReleaseAssetRenderResources(this);
}

void USkeletalMesh::PostLoad()
{
	Super::PostLoad();
	InitResources();
}

void USkeletalMesh::BeginDestroy()
{
	ReleaseResources();
	Super::BeginDestroy();
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
	LLM_SCOPE(ELLMTag::Meshes);
	Super::Serialize(Ar);
	Ar << BoundingBox;
	if (!SerializeBulkPayload(Ar, this, GeometryBulkData,
			[this](FArchive& PayloadAr)
			{
				RenderData.Serialize(PayloadAr);
				PayloadAr << BoneBoundsRadii;
			}))
	{
		UE_LOG(LogEngine, Error, "USkeletalMesh %s: damaged render data", *GetPathName());
		RenderData = FLPS2Mesh();
		BoneBoundsRadii.Empty();
	}
}

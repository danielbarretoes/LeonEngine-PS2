#pragma once

#include "CoreMinimal.h"
#include "MaterialShared.h"
#include "PrimitiveSceneProxy.h"

class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * A USkeletalMeshComponent for the renderer (UE: FSkeletalMeshSceneProxy): its mesh, the values of each section's
 * material (the component's override of the slot, else the mesh's, else the default material), and the dynamic data
 * the component sends before each frame (USkeletalMeshComponent::SendRenderDynamicData_Concurrent): the skin matrices
 * of the current pose and the pose's bounds, which the renderer culls with.
 */
class ENGINE_API FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FSkeletalMeshSceneProxy(const USkeletalMeshComponent* InComponent);

	/** The mesh asset (the renderer's GPU copy is keyed by it). */
	[[nodiscard]] const USkeletalMesh& GetSkeletalMesh() const
	{
		return *SkeletalMesh;
	}
	/** The material a section of the mesh's render data draws with. */
	[[nodiscard]] const FMaterial& GetSectionMaterial(int32 SectionIndex) const;

	/** One skin matrix per bone (InverseBindPose * component space); empty before the first pose (the bind pose). */
	[[nodiscard]] const TArray<FMatrix>& GetBoneMatrices() const
	{
		return BoneMatrices;
	}
	/** UE: the dynamic data a skinned mesh sends each frame: the skin matrices and the pose's local bounds. */
	void SetDynamicData(TArrayView<const FMatrix> InBoneMatrices, const FBox& InLocalBounds)
	{
		// Copied into the proxy's own array, which keeps its memory from frame to frame.
		BoneMatrices.SetNum(InBoneMatrices.Num(), false);
		for (int32 Bone = 0; Bone < InBoneMatrices.Num(); ++Bone)
		{
			BoneMatrices[Bone] = InBoneMatrices[Bone];
		}
		LocalBounds = InLocalBounds;
	}

	/**
	 * The pose's bounds in the world (the mesh's bind-pose box before the first pose): the renderer culls the mesh by
	 * them, in the world pass and in the view model pass (N15), and they size its blob shadow.
	 */
	[[nodiscard]] FBox GetWorldBounds() const override;

	/** The mesh and the maps of its materials. */
	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	USkeletalMesh* SkeletalMesh = nullptr;
	TArray<FMaterial> SectionMaterials;
	TArray<FMatrix> BoneMatrices;
	FBox LocalBounds = FBox(ForceInit);
};

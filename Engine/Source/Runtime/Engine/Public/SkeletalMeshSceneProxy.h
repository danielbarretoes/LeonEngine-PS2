#pragma once

#include "CoreMinimal.h"
#include "PrimitiveSceneProxy.h"
#include "SkeletalMesh.h"

class USkeletalMeshComponent;

/**
 * A USkeletalMeshComponent for the renderer (UE: FSkeletalMeshSceneProxy): its mesh and the skin matrices of the
 * current pose, which the component sends before each frame (USkeletalMeshComponent::SendRenderDynamicData_Concurrent).
 */
class ENGINE_API FSkeletalMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FSkeletalMeshSceneProxy(const USkeletalMeshComponent* InComponent);

	[[nodiscard]] const USkeletalMesh& GetSkeletalMesh() const
	{
		return *SkeletalMesh;
	}

	/** Skin matrices in the GL memory layout (uploaded as they are). */
	[[nodiscard]] const TArray<FMatrix>& GetBoneMatrices() const
	{
		return BoneMatrices;
	}
	/** UE: the dynamic data a skinned mesh sends each frame. */
	void SetBoneMatrices(const TArray<FMatrix>& InBoneMatrices)
	{
		BoneMatrices = InBoneMatrices;
	}

private:
	TSharedPtr<USkeletalMesh> SkeletalMesh;
	TArray<FMatrix> BoneMatrices;
};

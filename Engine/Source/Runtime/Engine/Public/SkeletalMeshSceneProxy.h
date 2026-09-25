#pragma once

#include "CoreMinimal.h"
#include "MaterialShared.h"
#include "PrimitiveSceneProxy.h"

class USkeletalMesh;
class USkeletalMeshComponent;

/**
 * A USkeletalMeshComponent for the renderer (UE: FSkeletalMeshSceneProxy): its mesh, the values of the mesh's material
 * (slot 0, else the default material) and the skin matrices of the current pose, which the component sends before
 * each frame (USkeletalMeshComponent::SendRenderDynamicData_Concurrent).
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
	/** The material of the mesh's slot 0, which draws the whole mesh (Leon: one section). */
	[[nodiscard]] const FMaterial& GetMaterial() const
	{
		return Material;
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

	/** The mesh and the maps of its material. */
	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	USkeletalMesh* SkeletalMesh = nullptr;
	FMaterial Material;
	TArray<FMatrix> BoneMatrices;
};

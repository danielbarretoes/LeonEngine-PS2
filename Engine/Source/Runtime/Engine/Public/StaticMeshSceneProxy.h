#pragma once

#include "CoreMinimal.h"
#include "LPS2Mesh.h"
#include "MaterialShared.h"
#include "PrimitiveSceneProxy.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * A UStaticMeshComponent for the renderer (UE: FStaticMeshSceneProxy): its mesh and the material of each mesh section,
 * in section order.
 */
class ENGINE_API FStaticMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FStaticMeshSceneProxy(const UStaticMeshComponent* InComponent);

	/** The mesh asset (the renderer's GPU copy is keyed by it). */
	[[nodiscard]] const UStaticMesh& GetStaticMesh() const
	{
		return *StaticMesh;
	}

	/** Mesh sections the renderer draws (one for a mesh without sections). */
	[[nodiscard]] int32 GetNumSections() const
	{
		return SectionMaterials.Num();
	}
	/** The material a section draws with (UStaticMeshComponent::GetMaterial of its slot). */
	[[nodiscard]] const FMaterial& GetSectionMaterial(int32 SectionIndex) const;

	/**
	 * The instance's baked vertex colours when it draws with static lighting (a Static component whose colours were
	 * baked for its mesh as it is), else null: the renderer then lights it per frame (Movable) or draws the mesh's own
	 * colours (Static, not baked).
	 */
	[[nodiscard]] const FLPS2ColorStreams* GetBakedVertexColors() const
	{
		return BakedVertexColors.IsEmpty() ? nullptr : &BakedVertexColors;
	}

	/** The mesh's local bounds through the transform. */
	[[nodiscard]] FBox GetWorldBounds() const override;

	/** The mesh and the maps of the section materials. */
	void AddReferencedObjects(FReferenceCollector& Collector) override;

private:
	UStaticMesh* StaticMesh = nullptr;
	TArray<FMaterial> SectionMaterials;
	/** A copy of the component's baked colours when they apply (GetBakedVertexColors). */
	FLPS2ColorStreams BakedVertexColors;
};

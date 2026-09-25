#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Material.h"
#include "PrimitiveSceneProxy.h"

class UStaticMeshComponent;

/**
 * A UStaticMeshComponent for the renderer (UE: FStaticMeshSceneProxy): its mesh and the material of each mesh section,
 * in section order.
 */
class ENGINE_API FStaticMeshSceneProxy : public FPrimitiveSceneProxy
{
public:
	explicit FStaticMeshSceneProxy(const UStaticMeshComponent* InComponent);

	[[nodiscard]] const UStaticMesh& GetStaticMesh() const
	{
		return *StaticMesh;
	}
	[[nodiscard]] const TSharedPtr<UStaticMesh>& GetStaticMeshShared() const
	{
		return StaticMesh;
	}

	/** Mesh sections the renderer draws (one for a mesh without sections). */
	[[nodiscard]] int32 GetNumSections() const
	{
		return SectionMaterials.Num();
	}
	/** The material a section draws with (UStaticMeshComponent::GetMaterial of its slot). */
	[[nodiscard]] const FMaterial& GetSectionMaterial(int32 SectionIndex) const;

	/** Shown, casting shadows, and one of the component's materials is an opaque lit shadow caster. */
	[[nodiscard]] bool IsShadowCaster() const
	{
		return IsShown() && CastsDynamicShadow() && bHasShadowCastingMaterial;
	}

	/** The mesh's local bounds through the transform. */
	[[nodiscard]] FBox GetWorldBounds() const;

private:
	TSharedPtr<UStaticMesh> StaticMesh;
	TArray<FMaterial> SectionMaterials;
	bool bHasShadowCastingMaterial = false;
};

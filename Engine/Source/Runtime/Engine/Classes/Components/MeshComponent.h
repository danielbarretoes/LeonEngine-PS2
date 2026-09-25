#pragma once

#include "Components/PrimitiveComponent.h"
#include "CoreMinimal.h"
#include "MaterialShared.h"
#include "MeshComponent.generated.h"

/**
 * A primitive drawn from a mesh with material slots (UE: UMeshComponent). The override materials replace the mesh's
 * own per slot. Materials are FMaterial values until P14 brings UMaterialInterface assets, so they are not UPROPERTYs.
 */
UCLASS(Abstract)
class ENGINE_API UMeshComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UMeshComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Overrides the material of a slot (UE: SetMaterial); a registered component's proxy is recreated. */
	virtual void SetMaterial(int32 ElementIndex, const FMaterial& Material);
	/** The material a slot draws with: its override, else the mesh's (UE: GetMaterial). */
	[[nodiscard]] virtual FMaterial GetMaterial(int32 ElementIndex) const;
	/** Number of material slots (UE: GetNumMaterials): the overrides here, the mesh's in subclasses. */
	[[nodiscard]] virtual int32 GetNumMaterials() const;
	/** True when the slot has an override. */
	[[nodiscard]] bool HasOverrideMaterial(int32 ElementIndex) const;
	/** Drops every override (UE: EmptyOverrideMaterials). */
	void EmptyOverrideMaterials();

protected:
	/** Per-slot overrides (UE: OverrideMaterials); an unset slot keeps the mesh's material. */
	TArray<FMaterial> OverrideMaterials;
	TArray<bool> OverrideMaterialSet;
};

#pragma once

#include "Components/PrimitiveComponent.h"
#include "CoreMinimal.h"
#include "MeshComponent.generated.h"

class UMaterialInterface;

/**
 * A primitive drawn from a mesh with material slots (UE: UMeshComponent). The override materials replace the mesh's
 * own per slot; they are UPROPERTYs, so the garbage collector keeps them while the component lives.
 */
UCLASS(Abstract)
class ENGINE_API UMeshComponent : public UPrimitiveComponent
{
	GENERATED_BODY()

public:
	UMeshComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Per-slot overrides (UE: OverrideMaterials); a null slot keeps the mesh's material. */
	UPROPERTY()
	TArray<UMaterialInterface*> OverrideMaterials;

	/** Overrides the material of a slot (UE: SetMaterial); a registered component's proxy is recreated. */
	virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material);
	/** The material a slot draws with: its override, else the mesh's, else null (UE: GetMaterial). */
	[[nodiscard]] virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const;
	/** Number of material slots (UE: GetNumMaterials): the overrides here, the mesh's in subclasses. */
	[[nodiscard]] virtual int32 GetNumMaterials() const;
	/** True when the slot has an override. */
	[[nodiscard]] bool HasOverrideMaterial(int32 ElementIndex) const;
	/** Drops every override (UE: EmptyOverrideMaterials). */
	void EmptyOverrideMaterials();
};

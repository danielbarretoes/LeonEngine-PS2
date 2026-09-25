#pragma once

#include "Components/MeshComponent.h"
#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshComponent.generated.h"

/**
 * Draws a static mesh at its component transform (UE: UStaticMeshComponent): the mesh and materials an actor shows,
 * attachable to a socket of another component (a weapon in a hand bone).
 *
 * The mesh is a UStaticMesh asset (CPU data; the renderer keeps its GPU copy), shared through FResourceCache until P14
 * makes UStaticMesh an asset UObject; then StaticMesh becomes a UPROPERTY. AStaticMeshActor's root is one: the
 * `.llev` reader spawns one per placed mesh.
 */
UCLASS()
class ENGINE_API UStaticMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	UStaticMeshComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/**
	 * Sets the mesh (UE: SetStaticMesh); a registered component's proxy and body take the new mesh. False when
	 * unchanged.
	 */
	bool SetStaticMesh(TSharedPtr<UStaticMesh> NewMesh);
	[[nodiscard]] UStaticMesh* GetStaticMesh() const
	{
		return StaticMesh.Get();
	}
	/** The mesh with its shared ownership (render and physics snapshots keep it alive). */
	[[nodiscard]] const TSharedPtr<UStaticMesh>& GetStaticMeshShared() const
	{
		return StaticMesh;
	}
	[[nodiscard]] bool HasValidMesh() const
	{
		return StaticMesh != nullptr && StaticMesh->Valid();
	}

	/** The override, else the mesh's material for the slot, else the default material. */
	[[nodiscard]] FMaterial GetMaterial(int32 ElementIndex) const override;
	[[nodiscard]] int32 GetNumMaterials() const override;

	/** The material of each mesh section, in section order: GetMaterial of the section's slot. */
	void GetSectionMaterials(TArray<FMaterial>& OutMaterials) const;

	/**
	 * True when one of the slots' materials casts shadows, is opaque and is lit (every slot up to GetNumMaterials, at
	 * least slot 0).
	 */
	[[nodiscard]] bool HasShadowCastingMaterial() const;

	/** A FStaticMeshSceneProxy for a valid mesh (UE: CreateSceneProxy). */
	[[nodiscard]] FPrimitiveSceneProxy* CreateSceneProxy() override;

private:
	TSharedPtr<UStaticMesh> StaticMesh;
};

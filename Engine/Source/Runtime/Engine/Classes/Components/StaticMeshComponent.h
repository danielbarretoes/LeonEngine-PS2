#pragma once

#include "Components/MeshComponent.h"
#include "CoreMinimal.h"
#include "StaticMesh.h"
#include "StaticMeshComponent.generated.h"

/**
 * Draws a static mesh at its component transform (UE: UStaticMeshComponent): the mesh and materials an actor shows,
 * attachable to a socket of another component (a weapon in a hand bone).
 *
 * The mesh is the renderer's UStaticMesh resource, shared through FResourceCache, until P14 makes UStaticMesh an asset
 * UObject; then StaticMesh becomes a UPROPERTY. AStaticMeshActor's root is one: the `.llev` reader spawns one per
 * placed mesh.
 */
UCLASS()
class ENGINE_API UStaticMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	UStaticMeshComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Sets the mesh (UE: SetStaticMesh). Returns false when it did not change. */
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

	/** Submits the mesh with the material of slot 0 at the component transform (scale included). */
	void SubmitDraw(FSceneRenderer& Renderer) const override;

private:
	TSharedPtr<UStaticMesh> StaticMesh;
};

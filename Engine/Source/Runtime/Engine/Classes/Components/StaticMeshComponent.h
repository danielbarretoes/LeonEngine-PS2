#pragma once

#include "Components/MeshComponent.h"
#include "CoreMinimal.h"
#include "MaterialShared.h"
#include "StaticMeshComponent.generated.h"

class UStaticMesh;

/**
 * Draws a static mesh at its component transform (UE: UStaticMeshComponent): the mesh and materials an actor shows,
 * attachable to a socket of another component (a weapon in a hand bone).
 *
 * The mesh is a UStaticMesh asset the component references (a UPROPERTY, so the garbage collector keeps it while the
 * component lives); the renderer keeps its GPU copy. AStaticMeshActor's root is one: a map holds one per placed mesh.
 */
UCLASS()
class ENGINE_API UStaticMeshComponent : public UMeshComponent
{
	GENERATED_BODY()

public:
	UStaticMeshComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The mesh (UE: StaticMesh). Change it with SetStaticMesh, which updates the proxy and the body. */
	UPROPERTY()
	UStaticMesh* StaticMesh = nullptr;

	/**
	 * Sets the mesh (UE: SetStaticMesh); a registered component's proxy and body take the new mesh. False when
	 * unchanged.
	 */
	bool SetStaticMesh(UStaticMesh* NewMesh);
	[[nodiscard]] UStaticMesh* GetStaticMesh() const
	{
		return StaticMesh;
	}
	/** A mesh with triangles is set. */
	[[nodiscard]] bool HasValidMesh() const;

	/** The override, else the mesh's material for the slot, else null (UE: GetMaterial). */
	[[nodiscard]] UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
	[[nodiscard]] int32 GetNumMaterials() const override;

	/**
	 * The values each mesh section draws with, in section order: the render proxy of GetMaterial of the section's
	 * slot, or of the engine's default material for a slot without one (UE: the proxy's fallback to
	 * UMaterial::GetDefaultMaterial).
	 */
	void GetSectionMaterials(TArray<FMaterial>& OutMaterials) const;

	/**
	 * True when one of the slots' materials casts shadows, is opaque and is lit (every slot up to GetNumMaterials, at
	 * least slot 0).
	 */
	[[nodiscard]] bool HasShadowCastingMaterial() const;

	/** A FStaticMeshSceneProxy for a valid mesh (UE: CreateSceneProxy). */
	[[nodiscard]] FPrimitiveSceneProxy* CreateSceneProxy() override;

	/** The world transform of a socket of the mesh; the component's for NAME_None or a missing socket (UE). */
	[[nodiscard]] FTransform GetSocketTransform(FName InSocketName) const override;
	/** True when the mesh has the socket (UE: DoesSocketExist). */
	[[nodiscard]] bool DoesSocketExist(FName InSocketName) const override;
};

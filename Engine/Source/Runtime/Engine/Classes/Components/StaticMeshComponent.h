#pragma once

#include "Components/MeshComponent.h"
#include "CoreMinimal.h"
#include "LPS2Mesh.h"
#include "MaterialShared.h"
#include "StaticMeshComponent.generated.h"

class UStaticMesh;

/**
 * Draws a static mesh at its component transform (UE: UStaticMeshComponent): the mesh and materials an actor shows,
 * attachable to a socket of another component (a weapon in a hand bone).
 *
 * The mesh is a UStaticMesh asset the component references (a UPROPERTY, so the garbage collector keeps it while the
 * component lives); the renderer keeps its GPU copy. AStaticMeshActor's root is one: a map holds one per placed mesh.
 *
 * Lighting (Docs/PLANS/ps2-shipping.md N22): a Static component draws with its baked vertex colours, the lighting
 * LeonEd bakes into a map (UE: a static component's static lighting, here per vertex), with no light computed per
 * frame; without them (never baked, or baked for another build of its mesh) it draws with the mesh's own colours. A
 * Movable one is lit per frame by the scene's lights.
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
	 * The baked lighting of this instance (UE: the LODData's OverrideVertexColors): its own colour streams for the
	 * mesh's LPS2 v2 batches, written by LeonEd's static lighting (FStaticLightingSystem) and saved after the
	 * component's transform. Empty when never baked.
	 */
	FLPS2ColorStreams BakedVertexColors;

	/** Whether BakedVertexColors were baked for the mesh as it is (a mesh rebuilt since needs a new bake). */
	[[nodiscard]] bool HasValidBakedVertexColors() const;
	/** Takes the baked colours and updates a registered component's proxy. */
	void SetBakedVertexColors(FLPS2ColorStreams&& InColors);

	/** The component's tail: the baked vertex colours (VER_LEON_BAKED_VERTEX_COLORS). */
	void Serialize(FArchive& Ar) override;

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
	/** The material of the slot the mesh's collision triangle FaceIndex has (its MaterialIndices); SectionIndex that
	 * slot. */
	[[nodiscard]] UMaterialInterface* GetMaterialFromCollisionFaceIndex(
		int32 FaceIndex, int32& SectionIndex) const override;

	/**
	 * The values each mesh section draws with, in section order: the render proxy of GetMaterial of the section's
	 * slot, or of the engine's default material for a slot without one (UE: the proxy's fallback to
	 * UMaterial::GetDefaultMaterial).
	 */
	void GetSectionMaterials(TArray<FMaterial>& OutMaterials) const;

	/** A FStaticMeshSceneProxy for a valid mesh (UE: CreateSceneProxy). */
	[[nodiscard]] FPrimitiveSceneProxy* CreateSceneProxy() override;

	/** The world transform of a socket of the mesh; the component's for NAME_None or a missing socket (UE). */
	[[nodiscard]] FTransform GetSocketTransform(FName InSocketName) const override;
	/** True when the mesh has the socket (UE: DoesSocketExist). */
	[[nodiscard]] bool DoesSocketExist(FName InSocketName) const override;
};

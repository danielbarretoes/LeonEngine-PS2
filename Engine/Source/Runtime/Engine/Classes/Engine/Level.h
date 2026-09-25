#pragma once

#include "CoreMinimal.h"
#include "Level/Light.h"
#include "Material.h"
#include "StaticMesh.h"
#include "UObject/Object.h"
#include "Level.generated.h"

class AActor;
class AWorldSettings;
class UWorld;

/**
 * What the renderer draws of one AStaticMeshActor this frame: its mesh, world transform and the material of each mesh
 * section (ULevel::GetStaticMeshSnapshots). P13's FScene replaces it with the component's scene proxy.
 */
struct ENGINE_API FLevelStaticMesh
{
	FTransform Transform;
	TSharedPtr<UStaticMesh> Mesh;
	/** The material of each mesh section, in section order (UStaticMeshComponent::GetMaterial of its slot). */
	TArray<FMaterial> SectionMaterials;
	/** Not drawn (the actor or the component is hidden); still in the debug bounds view. */
	bool bHidden = false;
	/** The component casts shadows and one of its materials is an opaque lit shadow caster. */
	bool bCastShadow = false;

	/** Model matrix (Transform with its scale). */
	[[nodiscard]] FMatrix EffectiveModelMatrix() const
	{
		return Transform.ToMatrixWithScale();
	}

	[[nodiscard]] int32 SubMeshCount() const;
	[[nodiscard]] const FMaterial& MaterialForSubMesh(int32 SubMeshIndex) const;
	[[nodiscard]] bool IsShadowCaster() const;
};

/**
 * A level of a world (UE: ULevel): its outer is the owning UWorld and it holds the world's actors in Actors (spawned
 * with the level as their outer, so the level keeps them alive through the garbage collector).
 *
 * A `.llev` file becomes actors of the persistent level (AStaticMeshActor, APlayerStart, the volumes, the lights,
 * ATargetPoint and AWorldSettings; see LeonLevelFormat.h). Their components give the physics scene its bodies
 * (UPrimitiveComponent::CreatePhysicsState). Until the FScene boundary, the renderer reads the actors through
 * GetStaticMeshSnapshots and GetLightSnapshots.
 */
UCLASS()
class ENGINE_API ULevel : public UObject
{
	GENERATED_BODY()

public:
	/** The world this level belongs to (UE: OwningWorld); null for a standalone level. */
	UPROPERTY(Transient)
	UWorld* OwningWorld = nullptr;

	/**
	 * The actors of the level, in spawn order (UE: Actors). An entry becomes null when its actor is destroyed during a
	 * world tick; the world compacts the array once the tick ends.
	 */
	UPROPERTY()
	TArray<AActor*> Actors;

	/** The level's settings actor, spawned by the level reader (UE: GetWorldSettings); null for a level without one. */
	[[nodiscard]] AWorldSettings* GetWorldSettings() const
	{
		return WorldSettings;
	}
	void SetWorldSettings(AWorldSettings* NewWorldSettings)
	{
		WorldSettings = NewWorldSettings;
	}

	/** The mesh of every live AStaticMeshActor, in actor order. */
	void GetStaticMeshSnapshots(TArray<FLevelStaticMesh>& OutMeshes) const;

	/** Every visible directional and point light actor, in actor order, as the renderer's light structs. */
	void GetLightSnapshots(TArray<FDirectionalLight>& OutDirectional, TArray<FPointLight>& OutPoint) const;

private:
	/** UE: WorldSettings. */
	UPROPERTY(Transient)
	AWorldSettings* WorldSettings = nullptr;
};

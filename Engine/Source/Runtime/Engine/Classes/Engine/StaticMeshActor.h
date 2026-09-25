#pragma once

#include "Components/StaticMeshComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "StaticMeshActor.generated.h"

/**
 * A static mesh placed in a level (UE: AStaticMeshActor): its root is the UStaticMeshComponent that draws the mesh and,
 * when its collision is enabled, gives the physics scene a body. The `.llev` reader spawns one per Cube, Sphere,
 * Plane and StaticMesh record.
 */
UCLASS()
class ENGINE_API AStaticMeshActor : public AActor
{
	GENERATED_BODY()

public:
	AStaticMeshActor(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: GetStaticMeshComponent. */
	[[nodiscard]] UStaticMeshComponent* GetStaticMeshComponent() const
	{
		return StaticMeshComponent;
	}

private:
	/** The root (UE: StaticMeshComponent, default subobject "StaticMeshComponent0"). */
	UPROPERTY()
	UStaticMeshComponent* StaticMeshComponent = nullptr;
};

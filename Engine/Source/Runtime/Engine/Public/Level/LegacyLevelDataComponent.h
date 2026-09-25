#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Level/LeonLevelFormat.h"
#include "LegacyLevelDataComponent.generated.h"

/**
 * The `.llev` bookkeeping the level saver needs to write a level back byte for byte (Leon only; it goes away with the
 * `.llev` format in P15). Everything a level means lives in its Unreal home (the movement components, the
 * UInteractableComponent, the world settings' DefaultGameMode, the ACameraActor of the framing): this only keeps how
 * the file wrote it. It is transient, so a map saved from a legacy level never holds it.
 *
 * - Every actor spawned from a record: the record class it came from.
 * - Static meshes and blocking volumes: the mesh and material keys as the file wrote them, the sphere tessellation.
 * - World settings: the level name and the game mode string.
 */
UCLASS(Transient)
class ENGINE_API ULegacyLevelDataComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	ULegacyLevelDataComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	ELevelActorClass ActorClass = ELevelActorClass::StaticMesh;

	FString MeshPath;
	FString MaterialPath;
	int32 SphereSegments = 24;
	int32 SphereRings = 16;

	FString LevelName;
	FString GameModeName;
};

#pragma once

#include "Camera/CameraComponent.h"
#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "Level/LeonLevelFormat.h"
#include "LegacyLevelDataComponent.generated.h"

/**
 * The `.llev` fields that have no Unreal home, kept on the actor the level reader spawned from the record so the
 * level saver writes the same file back (Leon only; UE has no counterpart). It goes away with the `.llev` format in
 * P15. Values are in world units (cm, degrees in UE's sense), like the rest of the engine.
 *
 * - Every level actor: the record class it came from.
 * - Static meshes and blocking volumes: the mesh and material keys as the file wrote them, the sphere tessellation,
 *   the spin and bob animation (read and saved, never played).
 * - Trigger volumes: the interact radius and cost, the game-defined payload and consume-on-use.
 * - Point lights: the orbit animation (read and saved, never played).
 * - World settings: the level name, the game mode string, the ignored environment map and the camera framing (also
 *   applied to the engine camera when the level loads).
 */
UCLASS()
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
	/** Degrees per second about Z. */
	float SpinYaw = 0.0f;
	bool bHasBob = false;
	/** cm */
	float BobBaseZ = 0.0f;
	/** cm */
	float BobAmplitude = 10.0f;
	float BobSpeed = 1.0f;

	int32 InteractCost = 0;
	/** cm */
	float InteractRadius = 200.0f;
	FString Payload;
	bool bConsumeOnUse = false;

	bool bHasOrbit = false;
	/** cm */
	float OrbitRadius = 100.0f;
	/** cm */
	float OrbitHeight = 100.0f;
	/** cm */
	float OrbitHeightAmp = 0.0f;
	float OrbitSpeed = 1.0f;

	FString LevelName;
	FString GameModeName;
	FString EnvironmentPath;
	float EnvironmentExposure = 1.0f;

	ECameraMode CameraMode = ECameraMode::Orbit;
	FVector CameraTarget = FVector::ZeroVector;
	FVector CameraEye = FVector::ZeroVector;
	/** cm */
	float CameraDistance = 500.0f;
	FRotator CameraViewRotation = FRotator::ZeroRotator;
};

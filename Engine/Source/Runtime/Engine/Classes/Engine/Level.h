#pragma once

#include "CoreMinimal.h"
#include "Level/Light.h"
#include "Material.h"
#include "StaticMesh.h"
#include "Texture2D.h"
#include "UObject/Object.h"
#include "Level.generated.h"

class AActor;
class UWorld;

/** UE-like mobility: Static never moves (baked lighting later); Movable may move at runtime. */
enum class EComponentMobility : uint8
{
	Static = 0,
	Movable = 1,
};

/**
 * Drawable placed mesh of a legacy .llev level (visual / collision proxy; not an actor or a UStaticMeshComponent: P13
 * turns these into AStaticMeshActors).
 * Resolution order in MaterialForSubMesh():
 *   1) Materials if non-empty (per-slot overrides)
 *   2) Material if bMaterialOverride (asset / JSON override replaced the MTL)
 *   3) mesh MTL materials
 *   4) Material (engine default checker when a procedural mesh has no MTL)
 */
struct ENGINE_API FLevelStaticMesh
{
	FTransform Transform;
	TSharedPtr<UStaticMesh> Mesh;
	FMaterial Material;
	TArray<FMaterial> Materials; // optional per-slot overrides
	/** When true, Material is used for every submesh (asset / inline overrode the MTL). */
	bool bMaterialOverride = false;

	/** Optional tag from the level ("tag"). Spawn points use FPlayerStart, not tagged meshes. */
	FString Tag;
	/** UE-like collision enabled: registers an FPhysScene body (Static unless simulating). */
	bool bCollisionEnabled = false;
	/** UE-like bSimulatePhysics: Dynamic body (implies collision). */
	bool bSimulatePhysics = false;
	/** UE-like bEnableGravity: applies to simulated bodies (default true). */
	bool bEnableGravity = true;
	/** When true, skipped by the renderer (UE-like BlockingVolume / HiddenInGame). */
	bool bHidden = false;

	/** Static = never moves; Movable = may move at runtime. */
	EComponentMobility Mobility = EComponentMobility::Static;

	/** Editor / save provenance (filled by the level loader; used by the level saver). */
	FString EditorClass; // "Cube", "Sphere", "Plane", "BlockingVolume", "StaticMesh", ...
	FString MeshPath; // relative mesh path when imported
	FString MaterialPath; // relative material path when set
	/** Optional spin (level spin yaw): yaw rate about Z, degrees per second; preserved for the save round trip. */
	float SpinYaw = 0.0f;
	int32 SphereSegments = 24;
	int32 SphereRings = 16;

	/** Optional bob animation (level bob) along Z; preserved for the save round trip. Lengths in cm. */
	bool bHasBob = false;
	float BobBaseZ = 0.0f;
	float BobAmplitude = 10.0f;
	float BobSpeed = 1.0f;

	/** Model matrix (Transform with its scale). */
	[[nodiscard]] FMatrix EffectiveModelMatrix() const
	{
		return Transform.ToMatrixWithScale();
	}

	[[nodiscard]] bool HasPhysicsBody() const
	{
		return bCollisionEnabled || bSimulatePhysics;
	}

	[[nodiscard]] int32 SubMeshCount() const;
	[[nodiscard]] const FMaterial& MaterialForSubMesh(int32 SubMeshIndex) const;
	[[nodiscard]] bool IsShadowCaster() const;
};

/** UE-like FPlayerStart: spawn transform for game-mode-possessed pawns (not a drawable mesh). */
struct ENGINE_API FPlayerStart
{
	FTransform Transform;
};

/** Interact / trigger volume (POD). Overlap tested in gameplay from the position + InteractRadius (cm). */
struct ENGINE_API FTriggerVolume
{
	FTransform Transform;
	float InteractRadius = 200.0f;
	int32 InteractCost = 0;
	FString Payload; // game-defined, e.g. Door, WallBuy:M14, Perk:Jugg
	FString Tag;
	bool bConsumeOnUse = false;
};

/** Damage volume (POD). AABB from the transform location and abs(Scale3D) * 50 cm (a scaled 100 cm basic cube). */
struct ENGINE_API FPainCausingVolume
{
	FTransform Transform; // location + scale of a basic cube (full size = abs(scale) * 100 cm)
	float DamagePerSecond = 12.f;
	float DamageInterval = 0.35f;
	FString Tag;
};

/** AI spawn marker (POD; not a drawable mesh). */
struct ENGINE_API FAISpawnPoint
{
	FTransform Transform;
	FString Tag;
};

/**
 * A level of a world (UE: ULevel): its outer is the owning UWorld and it holds the world's actors in Actors (spawned
 * with the level as their outer, so the level keeps them alive through the garbage collector).
 *
 * Until levels become actors (P13) it also carries the content of the legacy .llev level: static meshes, player starts,
 * volumes, AI spawn points and lights, which FSceneRenderer draws and FPhysScene turns into bodies.
 */
UCLASS()
class ENGINE_API ULevel : public UObject
{
	GENERATED_BODY()

public:
	/** The world this level belongs to (UE: OwningWorld); null for a standalone level (tests, level staging). */
	UPROPERTY(Transient)
	UWorld* OwningWorld = nullptr;

	/**
	 * The actors of the level, in spawn order (UE: Actors). An entry becomes null when its actor is destroyed during a
	 * world tick; the world compacts the array once the tick ends.
	 */
	UPROPERTY()
	TArray<AActor*> Actors;

	FLevelStaticMesh& AddStaticMesh(FLevelStaticMesh Component);
	FPlayerStart& AddPlayerStart(FPlayerStart Start);
	FTriggerVolume& AddTriggerVolume(FTriggerVolume Volume);
	FPainCausingVolume& AddPainCausingVolume(FPainCausingVolume Volume);
	FAISpawnPoint& AddAISpawnPoint(FAISpawnPoint Point);
	void ClearStaticMeshes();
	void ClearPlayerStarts();
	void ClearTriggerVolumes();
	void ClearPainCausingVolumes();
	void ClearAISpawnPoints();
	void ClearLights();
	/** Clears the legacy level content (not Actors). */
	void Clear();
	/** Replaces the legacy level content with Source's, which is left empty (a staged level load commits this way). */
	void MoveLevelContentFrom(ULevel& Source);

	[[nodiscard]] const TArray<FLevelStaticMesh>& GetStaticMeshes() const
	{
		return StaticMeshes;
	}
	[[nodiscard]] TArray<FLevelStaticMesh>& GetStaticMeshes()
	{
		return StaticMeshes;
	}

	[[nodiscard]] const TArray<FPlayerStart>& GetPlayerStarts() const
	{
		return PlayerStarts;
	}
	[[nodiscard]] TArray<FPlayerStart>& GetPlayerStarts()
	{
		return PlayerStarts;
	}

	[[nodiscard]] const TArray<FTriggerVolume>& GetTriggerVolumes() const
	{
		return TriggerVolumes;
	}
	[[nodiscard]] TArray<FTriggerVolume>& GetTriggerVolumes()
	{
		return TriggerVolumes;
	}

	[[nodiscard]] const TArray<FPainCausingVolume>& GetPainCausingVolumes() const
	{
		return PainCausingVolumes;
	}
	[[nodiscard]] TArray<FPainCausingVolume>& GetPainCausingVolumes()
	{
		return PainCausingVolumes;
	}

	[[nodiscard]] const TArray<FAISpawnPoint>& AISpawnPoints() const
	{
		return AiSpawnPoints;
	}
	[[nodiscard]] TArray<FAISpawnPoint>& AISpawnPoints()
	{
		return AiSpawnPoints;
	}

	/** First FPlayerStart, or nullptr if the level has none. */
	[[nodiscard]] const FPlayerStart* FindPlayerStart() const;

	/** First static mesh whose tag matches (case-sensitive), or Npos if none. */
	[[nodiscard]] SIZE_T FindStaticMeshIndexByTag(const FString& InTag) const;

	[[nodiscard]] const TArray<FDirectionalLight>& GetDirectionalLights() const
	{
		return DirectionalLights;
	}
	[[nodiscard]] TArray<FDirectionalLight>& GetDirectionalLights()
	{
		return DirectionalLights;
	}

	[[nodiscard]] const TArray<FPointLight>& GetPointLights() const
	{
		return PointLights;
	}
	[[nodiscard]] TArray<FPointLight>& GetPointLights()
	{
		return PointLights;
	}

	/** The name the .llev document gives the level (not the object name, which is "PersistentLevel"). */
	void SetLevelName(const FString& InName)
	{
		LevelName = InName;
	}
	[[nodiscard]] const FString& GetLevelName() const
	{
		return LevelName;
	}
	void SetGameMode(const FString& InGameMode)
	{
		GameMode = InGameMode;
	}
	[[nodiscard]] const FString& GetGameMode() const
	{
		return GameMode;
	}

	/** "No static mesh" index (the physics scene's NoLevelMeshIndex). */
	static constexpr SIZE_T Npos = static_cast<SIZE_T>(-1);

private:
	TArray<FLevelStaticMesh> StaticMeshes;
	TArray<FPlayerStart> PlayerStarts;
	TArray<FTriggerVolume> TriggerVolumes;
	TArray<FPainCausingVolume> PainCausingVolumes;
	TArray<FAISpawnPoint> AiSpawnPoints;
	TArray<FDirectionalLight> DirectionalLights{FDirectionalLight{}};
	TArray<FPointLight> PointLights;
	FString LevelName;
	FString GameMode;
};

#pragma once

#include "CoreMinimal.h"
#include "Level/LegacyTransform.h"
#include "Level/Light.h"
#include "Material.h"
#include "StaticMesh.h"
#include "Texture2D.h"

/** UE-like mobility: Static never moves (baked lighting later); Movable may move at runtime. */
enum class EComponentMobility : uint8
{
	Static = 0,
	Movable = 1,
};

/**
 * Drawable placed mesh in a level (visual / collision proxy; not a gameplay actor or UE's UStaticMeshComponent).
 * Resolution order in MaterialForSubMesh():
 *   1) Materials if non-empty (per-slot overrides)
 *   2) Material if bMaterialOverride (asset / JSON override replaced the MTL)
 *   3) mesh MTL materials
 *   4) Material (engine default checker when a procedural mesh has no MTL)
 */
struct ENGINE_API UStaticMeshComponent
{
	FLegacyTransform Transform;
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
	/** When true, the renderer uses ModelMatrixOverride instead of Transform.ModelMatrix(). */
	bool bUseModelMatrixOverride = false;
	FMatrix ModelMatrixOverride = FMatrix::Identity;

	/** Static = never moves; Movable = may move at runtime. */
	EComponentMobility Mobility = EComponentMobility::Static;

	/** Editor / save provenance (filled by the level loader; used by the level saver). */
	FString EditorClass; // "Cube", "Sphere", "Plane", "BlockingVolume", "StaticMesh", ...
	FString MeshPath; // relative mesh path when imported
	FString MaterialPath; // relative material path when set
	float SpinYaw = 0.0f;
	int32 SphereSegments = 24;
	int32 SphereRings = 16;

	/** Optional bob animation (level bob); preserved for the save round trip. */
	bool bHasBob = false;
	float BobBaseY = 0.0f;
	float BobAmplitude = 0.1f;
	float BobSpeed = 1.0f;

	/** Model matrix in the renderer's GL convention (LegacyGLMath.h). */
	[[nodiscard]] FMatrix EffectiveModelMatrix() const
	{
		return bUseModelMatrixOverride ? ModelMatrixOverride : Transform.ModelMatrix();
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
	FLegacyTransform Transform;
};

/** Interact / trigger volume (POD). Overlap tested in gameplay from the position + InteractRadius. */
struct ENGINE_API FTriggerVolume
{
	FLegacyTransform Transform;
	float InteractRadius = 2.f;
	int32 InteractCost = 0;
	FString Payload; // game-defined, e.g. Door, WallBuy:M14, Perk:Jugg
	FString Tag;
	bool bConsumeOnUse = false;
};

/** Damage volume (POD). AABB from Transform.Position and abs(Scale) * 0.5. */
struct ENGINE_API FPainCausingVolume
{
	FLegacyTransform Transform; // position + scale as half-extents box (full size = abs(scale))
	float DamagePerSecond = 12.f;
	float DamageInterval = 0.35f;
	FString Tag;
};

/** AI spawn marker (POD; not a drawable mesh). */
struct ENGINE_API FAISpawnPoint
{
	FLegacyTransform Transform;
	FString Tag;
};

/**
 * Map content container (UE-style ULevel): static mesh components + lights. Distinct from the gameplay UWorld
 * (spawned actors). The application owns the contents; FSceneRenderer reads them.
 */
class ENGINE_API ULevel
{
public:
	UStaticMeshComponent& AddStaticMesh(UStaticMeshComponent Component);
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
	void Clear();

	[[nodiscard]] const TArray<UStaticMeshComponent>& GetStaticMeshes() const
	{
		return StaticMeshes;
	}
	[[nodiscard]] TArray<UStaticMeshComponent>& GetStaticMeshes()
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

	void SetName(const FString& InName)
	{
		Name = InName;
	}
	[[nodiscard]] const FString& GetName() const
	{
		return Name;
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
	TArray<UStaticMeshComponent> StaticMeshes;
	TArray<FPlayerStart> PlayerStarts;
	TArray<FTriggerVolume> TriggerVolumes;
	TArray<FPainCausingVolume> PainCausingVolumes;
	TArray<FAISpawnPoint> AiSpawnPoints;
	TArray<FDirectionalLight> DirectionalLights{FDirectionalLight{}};
	TArray<FPointLight> PointLights;
	FString Name;
	FString GameMode;
};

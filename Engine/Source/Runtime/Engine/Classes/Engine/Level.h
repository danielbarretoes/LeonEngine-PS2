#pragma once

#include "Level/Light.h"
#include "Material.h"
#include "Migration/LegacyTransform.h"
#include "StaticMesh.h"
#include "Texture2D.h"

#include <glm/mat4x4.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

/// Unreal-like mobility: Static never moves (baked lighting later); Movable may move at runtime.
enum class EComponentMobility : std::uint8_t
{
	Static = 0,
	Movable = 1,
};

/// Drawable placed mesh in a Level (visual / collision proxy — not a gameplay `Actor` or
/// Unreal `UStaticMeshComponent`). Named for level JSON familiarity.
/// Resolution order in materialForSubMesh():
///   1) materials[] if non-empty (per-slot overrides)
///   2) material if materialOverride (asset / JSON override replaced MTL)
///   3) mesh MTL materials
///   4) material (engine default checker when procedural mesh has no MTL)
struct ENGINE_API UStaticMeshComponent
{
	FLegacyTransform Transform;
	std::shared_ptr<UStaticMesh> Mesh;
	FMaterial Material;
	std::vector<FMaterial> Materials; // optional per-slot overrides
	/// When true, `material` is used for every submesh (asset/inline overrode MTL).
	bool bMaterialOverride = false;

	/// Optional tag from JSON (`"tag"`). Spawn points use FPlayerStart, not tagged meshes.
	std::string Tag;
	/// Unreal-like collision enabled — registers a FPhysScene body (Static unless simulating).
	bool bCollisionEnabled = false;
	/// Unreal-like `bSimulatePhysics` — Dynamic body (implies collision).
	bool bSimulatePhysics = false;
	/// Unreal-like `bEnableGravity` — applies to simulatePhysics bodies (default true).
	bool bEnableGravity = true;
	/// When true, skipped by the renderer (Unreal-like BlockingVolume / HiddenInGame).
	bool bHidden = false;
	/// When true, renderer uses `modelMatrixOverride` instead of `transform.modelMatrix()`.
	bool bUseModelMatrixOverride = false;
	glm::mat4 ModelMatrixOverride{1.0f};

	/// Static = never moves; Movable = may move at runtime.
	EComponentMobility Mobility = EComponentMobility::Static;

	/// Editor / save provenance (filled by LevelLoader; used by LevelSaver).
	std::string EditorClass; // "Cube", "Sphere", "Plane", "BlockingVolume", "StaticMesh", ...
	std::string MeshPath; // relative mesh path when imported
	std::string MaterialPath; // relative material JSON path when set
	float SpinYaw = 0.0f;
	int SphereSegments = 24;
	int SphereRings = 16;

	/// Optional bob animation (Level JSON `bob`); preserved for save round-trip.
	bool bHasBob = false;
	float BobBaseY = 0.0f;
	float BobAmplitude = 0.1f;
	float BobSpeed = 1.0f;

	[[nodiscard]] glm::mat4 EffectiveModelMatrix() const
	{
		return bUseModelMatrixOverride ? ModelMatrixOverride : Transform.ModelMatrix();
	}

	[[nodiscard]] bool HasPhysicsBody() const
	{
		return bCollisionEnabled || bSimulatePhysics;
	}

	[[nodiscard]] std::size_t SubMeshCount() const;
	[[nodiscard]] const FMaterial& MaterialForSubMesh(std::size_t SubMeshIndex) const;
	[[nodiscard]] bool IsShadowCaster() const;
};

/// Unreal-like FPlayerStart — spawn transform for GameMode-possessed pawns (not a drawable mesh).
struct ENGINE_API FPlayerStart
{
	FLegacyTransform Transform{};
};

/// Interact / trigger volume (POD). Overlap tested in gameplay from position + interactRadius.
struct ENGINE_API FTriggerVolume
{
	FLegacyTransform Transform{};
	float InteractRadius = 2.f;
	int InteractCost = 0;
	std::string Payload; // game-defined e.g. Door, WallBuy:M14, Perk:Jugg
	std::string Tag;
	bool bConsumeOnUse = false;
};

/// Damage volume (POD). AABB from transform.position and abs(scale) * 0.5.
struct ENGINE_API FPainCausingVolume
{
	FLegacyTransform Transform{}; // position + scale as half-extents box (full size = abs(scale))
	float DamagePerSecond = 12.f;
	float DamageInterval = 0.35f;
	std::string Tag;
};

/// AI spawn marker (POD — not a drawable mesh).
struct ENGINE_API FAISpawnPoint
{
	FLegacyTransform Transform{};
	std::string Tag;
};

/// Map content container (Unreal-style Level / ULevel): StaticMeshComponents + lights + env.
/// Distinct from gameplay `World` (spawned Actors). The app owns contents; FSceneRenderer reads them.
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

	[[nodiscard]] const std::vector<UStaticMeshComponent>& GetStaticMeshes() const
	{
		return StaticMeshes;
	}
	[[nodiscard]] std::vector<UStaticMeshComponent>& GetStaticMeshes()
	{
		return StaticMeshes;
	}

	[[nodiscard]] const std::vector<FPlayerStart>& GetPlayerStarts() const
	{
		return PlayerStarts;
	}
	[[nodiscard]] std::vector<FPlayerStart>& GetPlayerStarts()
	{
		return PlayerStarts;
	}

	[[nodiscard]] const std::vector<FTriggerVolume>& GetTriggerVolumes() const
	{
		return TriggerVolumes;
	}
	[[nodiscard]] std::vector<FTriggerVolume>& GetTriggerVolumes()
	{
		return TriggerVolumes;
	}

	[[nodiscard]] const std::vector<FPainCausingVolume>& GetPainCausingVolumes() const
	{
		return PainCausingVolumes;
	}
	[[nodiscard]] std::vector<FPainCausingVolume>& GetPainCausingVolumes()
	{
		return PainCausingVolumes;
	}

	[[nodiscard]] const std::vector<FAISpawnPoint>& AISpawnPoints() const
	{
		return AiSpawnPoints;
	}
	[[nodiscard]] std::vector<FAISpawnPoint>& AISpawnPoints()
	{
		return AiSpawnPoints;
	}

	/// First FPlayerStart, or nullptr if the level has none.
	[[nodiscard]] const FPlayerStart* FindPlayerStart() const;

	/// First static mesh whose tag matches, or npos if none.
	[[nodiscard]] std::size_t FindStaticMeshIndexByTag(std::string_view InTag) const;

	[[nodiscard]] const std::vector<FDirectionalLight>& GetDirectionalLights() const
	{
		return DirectionalLights;
	}
	[[nodiscard]] std::vector<FDirectionalLight>& GetDirectionalLights()
	{
		return DirectionalLights;
	}

	[[nodiscard]] const std::vector<FPointLight>& GetPointLights() const
	{
		return PointLights;
	}
	[[nodiscard]] std::vector<FPointLight>& GetPointLights()
	{
		return PointLights;
	}

	void SetName(std::string InName)
	{
		Name = std::move(InName);
	}
	[[nodiscard]] const std::string& GetName() const
	{
		return Name;
	}
	void SetGameMode(std::string InGameMode)
	{
		GameMode = std::move(InGameMode);
	}
	[[nodiscard]] const std::string& GetGameMode() const
	{
		return GameMode;
	}

	static constexpr std::size_t Npos = (std::numeric_limits<std::size_t>::max)();

private:
	std::vector<UStaticMeshComponent> StaticMeshes;
	std::vector<FPlayerStart> PlayerStarts;
	std::vector<FTriggerVolume> TriggerVolumes;
	std::vector<FPainCausingVolume> PainCausingVolumes;
	std::vector<FAISpawnPoint> AiSpawnPoints;
	std::vector<FDirectionalLight> DirectionalLights{FDirectionalLight{}};
	std::vector<FPointLight> PointLights;
	std::string Name;
	std::string GameMode;
};

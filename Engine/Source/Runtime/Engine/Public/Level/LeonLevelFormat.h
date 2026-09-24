#pragma once

#include "Camera/CameraComponent.h"
#include "Engine/Level.h"

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>
#include <vector>

class UGameEngine;
struct FLevelAnimation;

/// Binary Leon Level container (`.llev`): little-endian, string-table based.
/// Layout: header → string table → meta → camera → actors → lights.
inline constexpr std::uint32_t LeonLevelMagic = 0x56454C4Cu; // 'L','L','E','V'
inline constexpr std::uint32_t LeonLevelVersion = 2u;
inline constexpr const char* LeonLevelExtension = ".llev";

/// Actor archetypes storable in a `.llev` (matches the on-disk `u8 class`).
enum class ELevelActorClass : std::uint8_t
{
	PlayerStart = 0,
	Cube = 1,
	Sphere = 2,
	Plane = 3,
	BlockingVolume = 4,
	StaticMesh = 5,
	TriggerVolume = 6,
	PainCausingVolume = 7,
	AISpawnPoint = 8,
};

/// Light archetypes storable in a `.llev` (matches the on-disk `u8 class`).
enum class ELevelLightClass : std::uint8_t
{
	DirectionalLight = 0,
	PointLight = 1,
};

// Actor `u32 flags` bitfield.
inline constexpr std::uint32_t LevelActorFlagCollisionEnabled = 1u << 0;
inline constexpr std::uint32_t LevelActorFlagSimulatePhysics = 1u << 1;
inline constexpr std::uint32_t LevelActorFlagEnableGravity = 1u << 2;
inline constexpr std::uint32_t LevelActorFlagHidden = 1u << 3;
inline constexpr std::uint32_t LevelActorFlagHasBob = 1u << 4;
inline constexpr std::uint32_t LevelActorFlagHasSpinYaw = 1u << 5;
inline constexpr std::uint32_t LevelActorFlagHasFitHeight = 1u << 6;
inline constexpr std::uint32_t LevelActorFlagHasMaterial = 1u << 7;
inline constexpr std::uint32_t LevelActorFlagHasMesh = 1u << 8;
inline constexpr std::uint32_t LevelActorFlagHasLightmapId = 1u << 9;
inline constexpr std::uint32_t LevelActorFlagHasLightmapPath = 1u << 10;
inline constexpr std::uint32_t LevelActorFlagHasTag = 1u << 11;
inline constexpr std::uint32_t LevelActorFlagHasInteractCost = 1u << 12;
inline constexpr std::uint32_t LevelActorFlagHasPainData = 1u << 13;
inline constexpr std::uint32_t LevelActorFlagHasPayload = 1u << 14;
inline constexpr std::uint32_t LevelActorFlagConsumeOnUse = 1u << 15;

// Light `u32 flags` bitfield.
inline constexpr std::uint32_t LevelLightFlagCastShadows = 1u << 0;
inline constexpr std::uint32_t LevelLightFlagHasOrbit = 1u << 1;

/// One placed actor as stored in a `.llev` (no GPU / resource handles).
struct ENGINE_API FLevelActorRecord
{
	ELevelActorClass ActorClass = ELevelActorClass::StaticMesh;
	EComponentMobility Mobility = EComponentMobility::Static;

	bool bCollisionEnabled = false;
	bool bSimulatePhysics = false;
	bool bEnableGravity = true;
	bool bHidden = false;

	glm::vec3 Position{0.0f, 0.0f, 0.0f};
	glm::vec3 RotationDegrees{0.0f, 0.0f, 0.0f};
	glm::vec3 Scale{1.0f, 1.0f, 1.0f};

	std::string Tag;
	std::string MaterialPath; // `.lmat` path (empty = mesh / default material)
	std::string MeshPath; // imported mesh path (UStaticMesh only)
	std::string LightmapId;
	std::string LightmapPath;
	std::uint32_t LightmapResolution = 128;

	// Sphere tessellation (written only when `actorClass == Sphere`).
	std::int32_t SphereSegments = 24;
	std::int32_t SphereRings = 16;

	bool bHasSpinYaw = false;
	float SpinYaw = 0.0f;

	bool bHasBob = false;
	float BobBaseY = 0.0f;
	float BobAmplitude = 0.1f;
	float BobSpeed = 1.0f;

	bool bHasFitHeight = false;
	float FitHeight = 0.0f;

	// FTriggerVolume / interactables (written when HasInteractCost / HasPayload / ConsumeOnUse).
	int InteractCost = 0;
	float InteractRadius = 2.0f;
	float DamagePerSecond = 12.0f;
	float DamageInterval = 0.35f;
	std::string Payload;
	bool bConsumeOnUse = false;
};

/// One placed light as stored in a `.llev`.
struct ENGINE_API FLevelLightRecord
{
	ELevelLightClass LightClass = ELevelLightClass::DirectionalLight;
	bool bCastShadows = true;
	bool bHasOrbit = false;

	glm::vec3 Position{0.0f, 0.0f, 0.0f};
	glm::vec3 RotationDegrees{0.0f, 0.0f, 0.0f};
	glm::vec3 LightColor{1.0f, 1.0f, 1.0f};

	float Intensity = 1.0f;
	float Range = 8.0f;
	float SourceAngle = DefaultLightSourceAngleDegrees;

	float OrbitRadius = 1.0f;
	float OrbitHeight = 1.0f;
	float OrbitHeightAmp = 0.0f;
	float OrbitSpeed = 1.0f;
};

/// Camera framing stored in a `.llev` (always present).
struct ENGINE_API FLevelCameraRecord
{
	ECameraMode Mode = ECameraMode::Orbit;
	glm::vec3 Target{0.0f, 0.0f, 0.0f};
	glm::vec3 Eye{0.0f, 0.0f, 0.0f};
	float Distance = 5.0f;
	float Yaw = 45.0f;
	float Pitch = 25.0f;
};

/// In-memory mirror of a `.llev` file: plain data, no engine resources resolved yet.
struct ENGINE_API FLevelDocument
{
	std::string Name;
	std::string GameMode;
	std::string EnvironmentPath;
	float EnvironmentExposure = 1.0f;

	FLevelCameraRecord Camera;
	std::vector<FLevelActorRecord> Actors;
	std::vector<FLevelLightRecord> Lights;
};

/// Snapshot a live Level + Camera into a serializable document.
[[nodiscard]] FLevelDocument BuildLevelDocument(const ULevel& Level, const UCameraComponent& InCamera);

/// Encode a document as `.llev` bytes.
[[nodiscard]] std::vector<std::uint8_t> SerializeLeonLevel(const FLevelDocument& Doc);

/// Decode `.llev` bytes; returns false on bad magic / version / truncation.
[[nodiscard]] bool DeserializeLeonLevel(const std::vector<std::uint8_t>& InBytes, FLevelDocument& Out);

/// Write a document to `path` as `.llev`.
[[nodiscard]] bool SaveLeonLevelFile(const std::string& Path, const FLevelDocument& Doc);

/// Read a `.llev` file into `out`.
[[nodiscard]] bool LoadLeonLevelFile(const std::string& Path, FLevelDocument& Out);

/// Resolve a document into the Engine: builds a staging Level, commits on full success only,
/// then hydrates persisted lightmaps relative to `sourcePath`.
[[nodiscard]] bool ApplyLevelDocument(
	UGameEngine& Engine, const FLevelDocument& Doc, const std::string& SourcePath, FLevelAnimation* OutAnim = nullptr);

/// Resolve a content-relative key (`Materials/M_Floor.lmat`) for a level under `…/Content/Levels/`
/// (or legacy `…/Levels/`).
/// Prefers `<pack>/key`, then global `FPaths::ResolveAssetPath`.
[[nodiscard]] std::string ResolveLevelAssetPath(const std::string& LevelPath, const std::string& RelativeOrKey);

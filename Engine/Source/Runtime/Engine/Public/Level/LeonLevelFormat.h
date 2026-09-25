#pragma once

#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "Engine/Level.h"

class UGameEngine;

/**
 * Binary Leon Level container (.llev): little-endian, string-table based.
 * Layout: header → string table → meta → camera → actors → lights.
 */
inline constexpr uint32 LeonLevelMagic = 0x56454C4Cu; // 'L','L','E','V'
inline constexpr uint32 LeonLevelVersion = 2u;
inline constexpr const TCHAR* LeonLevelExtension = ".llev";

/** Actor archetypes storable in a .llev (matches the on-disk u8 class). */
enum class ELevelActorClass : uint8
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

/** Light archetypes storable in a .llev (matches the on-disk u8 class). */
enum class ELevelLightClass : uint8
{
	DirectionalLight = 0,
	PointLight = 1,
};

// Actor `u32 flags` bitfield.
inline constexpr uint32 LevelActorFlagCollisionEnabled = 1u << 0;
inline constexpr uint32 LevelActorFlagSimulatePhysics = 1u << 1;
inline constexpr uint32 LevelActorFlagEnableGravity = 1u << 2;
inline constexpr uint32 LevelActorFlagHidden = 1u << 3;
inline constexpr uint32 LevelActorFlagHasBob = 1u << 4;
inline constexpr uint32 LevelActorFlagHasSpinYaw = 1u << 5;
inline constexpr uint32 LevelActorFlagHasFitHeight = 1u << 6;
inline constexpr uint32 LevelActorFlagHasMaterial = 1u << 7;
inline constexpr uint32 LevelActorFlagHasMesh = 1u << 8;
inline constexpr uint32 LevelActorFlagHasLightmapId = 1u << 9;
inline constexpr uint32 LevelActorFlagHasLightmapPath = 1u << 10;
inline constexpr uint32 LevelActorFlagHasTag = 1u << 11;
inline constexpr uint32 LevelActorFlagHasInteractCost = 1u << 12;
inline constexpr uint32 LevelActorFlagHasPainData = 1u << 13;
inline constexpr uint32 LevelActorFlagHasPayload = 1u << 14;
inline constexpr uint32 LevelActorFlagConsumeOnUse = 1u << 15;

// Light `u32 flags` bitfield.
inline constexpr uint32 LevelLightFlagCastShadows = 1u << 0;
inline constexpr uint32 LevelLightFlagHasOrbit = 1u << 1;

/** One placed actor as stored in a .llev (no GPU / resource handles). */
struct ENGINE_API FLevelActorRecord
{
	ELevelActorClass ActorClass = ELevelActorClass::StaticMesh;
	EComponentMobility Mobility = EComponentMobility::Static;

	bool bCollisionEnabled = false;
	bool bSimulatePhysics = false;
	bool bEnableGravity = true;
	bool bHidden = false;

	FVector Position = FVector::ZeroVector;
	FVector RotationDegrees = FVector::ZeroVector;
	FVector Scale = FVector::OneVector;

	FString Tag;
	FString MaterialPath; // `.lmat` path (empty = mesh / default material)
	FString MeshPath; // imported mesh path (UStaticMesh only)
	FString LightmapId;
	FString LightmapPath;
	uint32 LightmapResolution = 128;

	// Sphere tessellation (written only when `actorClass == Sphere`).
	int32 SphereSegments = 24;
	int32 SphereRings = 16;

	bool bHasSpinYaw = false;
	float SpinYaw = 0.0f;

	bool bHasBob = false;
	float BobBaseY = 0.0f;
	float BobAmplitude = 0.1f;
	float BobSpeed = 1.0f;

	bool bHasFitHeight = false;
	float FitHeight = 0.0f;

	// FTriggerVolume / interactables (written when HasInteractCost / HasPayload / ConsumeOnUse).
	int32 InteractCost = 0;
	float InteractRadius = 2.0f;
	float DamagePerSecond = 12.0f;
	float DamageInterval = 0.35f;
	FString Payload;
	bool bConsumeOnUse = false;
};

/** One placed light as stored in a .llev. */
struct ENGINE_API FLevelLightRecord
{
	ELevelLightClass LightClass = ELevelLightClass::DirectionalLight;
	bool bCastShadows = true;
	bool bHasOrbit = false;

	FVector Position = FVector(0.0f, 0.0f, 0.0f);
	FVector RotationDegrees = FVector(0.0f, 0.0f, 0.0f);
	FVector LightColor = FVector(1.0f, 1.0f, 1.0f);

	float Intensity = 1.0f;
	float Range = 8.0f;
	float SourceAngle = DefaultLightSourceAngleDegrees;

	float OrbitRadius = 1.0f;
	float OrbitHeight = 1.0f;
	float OrbitHeightAmp = 0.0f;
	float OrbitSpeed = 1.0f;
};

/** Camera framing stored in a .llev (always present). */
struct ENGINE_API FLevelCameraRecord
{
	ECameraMode Mode = ECameraMode::Orbit;
	FVector Target = FVector(0.0f, 0.0f, 0.0f);
	FVector Eye = FVector(0.0f, 0.0f, 0.0f);
	float Distance = 5.0f;
	float Yaw = 45.0f;
	float Pitch = 25.0f;
};

/** In-memory mirror of a .llev file: plain data, no engine resources resolved yet. */
struct ENGINE_API FLevelDocument
{
	FString Name;
	FString GameMode;
	FString EnvironmentPath;
	float EnvironmentExposure = 1.0f;

	FLevelCameraRecord Camera;
	TArray<FLevelActorRecord> Actors;
	TArray<FLevelLightRecord> Lights;
};

/** Snapshot a live Level + Camera into a serializable document. */
[[nodiscard]] FLevelDocument BuildLevelDocument(const ULevel& Level, const UCameraComponent& InCamera);

/** Encode a document as .llev bytes. */
[[nodiscard]] TArray<uint8> SerializeLeonLevel(const FLevelDocument& Doc);

/** Decode .llev bytes; returns false on bad magic / version / truncation. */
[[nodiscard]] bool DeserializeLeonLevel(const TArray<uint8>& InBytes, FLevelDocument& Out);

/** Write a document to path as .llev. */
[[nodiscard]] bool SaveLeonLevelFile(const FString& Path, const FLevelDocument& Doc);

/** Read a .llev file into out. */
[[nodiscard]] bool LoadLeonLevelFile(const FString& Path, FLevelDocument& Out);

/**
 * Resolve a document into the Engine: builds a staging Level and commits on full success only; asset paths
 * resolve relative to SourcePath.
 */
[[nodiscard]] bool ApplyLevelDocument(UGameEngine& Engine, const FLevelDocument& Doc, const FString& SourcePath);

/**
 * Resolve a content-relative key (Materials/M_Floor.lmat) for a level under …/Content/Levels/
 * (or legacy …/Levels/).
 * Prefers <pack>/key, then global FPaths::ResolveLegacyContentPath.
 */
[[nodiscard]] FString ResolveLevelAssetPath(const FString& LevelPath, const FString& RelativeOrKey);

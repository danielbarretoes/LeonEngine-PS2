#pragma once

#include "Camera/CameraComponent.h"
#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "Level/Light.h"

class UWorld;
class ULevel;

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
	FString MaterialPath; // material key, a `.lmat` path (empty = mesh / default material): ResolveLevelAssetObjectPath
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

	// Trigger volumes / interactables (written when HasInteractCost / HasPayload / ConsumeOnUse).
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

/**
 * In-memory mirror of a .llev file: plain data, no engine resources resolved yet. Every record keeps the file's legacy
 * values (metres, Y up, right-handed, legacy angles); ApplyLevelDocument and BuildLevelDocument convert with
 * FLegacyCoordinateConversion.
 */
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

/**
 * Snapshot a live level + camera into a serializable document: the level's world settings, player starts, target
 * points, trigger and pain volumes, static meshes and blocking volumes, then directional and point lights, each group
 * in spawn order (the grouping the format always wrote). The animations and the trigger data come from their
 * components (URotatingMovementComponent, UBobbingMovementComponent, UOrbitMovementComponent,
 * UInteractableComponent), the record classes, content keys and level strings from each actor's
 * ULegacyLevelDataComponent.
 */
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
 * Resolve a document into a world as actors (the records' mesh and material keys name `.lasset` packages, loaded with
 * LoadObject: ResolveLevelAssetObjectPath; the basic shapes are /Engine/BasicShapes meshes and a record without a
 * material gets the engine's default material). Every mesh and material is resolved first; on a failure nothing
 * changes. Then the previous level-content actors are destroyed and the document spawns an AWorldSettings, one actor
 * per record in record order (APlayerStart, AStaticMeshActor for Cube / Sphere / Plane / StaticMesh, ABlockingVolume,
 * ATriggerVolume with a UInteractableComponent, APainCausingVolume, ATargetPoint for AISpawnPoint; a spin becomes a
 * URotatingMovementComponent, a bob a UBobbingMovementComponent), its lights (ADirectionalLight / APointLight, an orbit
 * a UOrbitMovementComponent, the default sun when it has no directional light) and an ACameraActor holding the camera
 * framing. The game mode name becomes the world settings' DefaultGameMode (plan decision D18: empty or "Default" leaves
 * it to the project). Collects garbage (a safe point).
 */
[[nodiscard]] bool ApplyLevelDocument(UWorld& World, const FLevelDocument& Doc, const FString& SourcePath);

/**
 * The game mode class a legacy level's game mode string names (Leon, plan decision D18): none for an empty string or
 * "Default" (the project's GlobalDefaultGameMode decides), else a class path or an alias of GameModeClassAliases
 * (UGameMapsSettings::GetGameModeForName); a name that is no game mode class is a warning and none.
 */
[[nodiscard]] UClass* ResolveLegacyLevelGameMode(const FString& GameModeName);

/**
 * The object path of the package a record's content key names (`Materials/M_Floor.lmat`, `Meshes/Crate.lmesh`): the
 * key is relative to the level's content root, the folder above its `Levels/` folder (`<Root>/Levels/X.llev`), and
 * resolves as FLegacyAssetKeys::ResolveKey does (the migrated package under the content root's mount point, a mount
 * point named after a folder outside them, then /Game, then /Engine). Empty for an empty key or no package.
 */
[[nodiscard]] FString ResolveLevelAssetObjectPath(const FString& LevelPath, const FString& Key);

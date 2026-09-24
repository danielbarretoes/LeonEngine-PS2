#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include "Camera/Camera.h"
#include "Engine/Level.h"
#include <string>
#include <vector>

namespace leon {

class Engine;
struct LevelAnimation;

/// Binary Leon Level container (`.llev`): little-endian, string-table based.
/// Layout: header → string table → meta → camera → actors → lights.
inline constexpr std::uint32_t kLeonLevelMagic = 0x56454C4Cu; // 'L','L','E','V'
inline constexpr std::uint32_t kLeonLevelVersion = 2u;
inline constexpr const char* kLeonLevelExtension = ".llev";

/// Actor archetypes storable in a `.llev` (matches the on-disk `u8 class`).
enum class ELevelActorClass : std::uint8_t {
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
enum class ELevelLightClass : std::uint8_t {
    DirectionalLight = 0,
    PointLight = 1,
};

// Actor `u32 flags` bitfield.
inline constexpr std::uint32_t kLevelActorFlagCollisionEnabled = 1u << 0;
inline constexpr std::uint32_t kLevelActorFlagSimulatePhysics = 1u << 1;
inline constexpr std::uint32_t kLevelActorFlagEnableGravity = 1u << 2;
inline constexpr std::uint32_t kLevelActorFlagHidden = 1u << 3;
inline constexpr std::uint32_t kLevelActorFlagHasBob = 1u << 4;
inline constexpr std::uint32_t kLevelActorFlagHasSpinYaw = 1u << 5;
inline constexpr std::uint32_t kLevelActorFlagHasFitHeight = 1u << 6;
inline constexpr std::uint32_t kLevelActorFlagHasMaterial = 1u << 7;
inline constexpr std::uint32_t kLevelActorFlagHasMesh = 1u << 8;
inline constexpr std::uint32_t kLevelActorFlagHasLightmapId = 1u << 9;
inline constexpr std::uint32_t kLevelActorFlagHasLightmapPath = 1u << 10;
inline constexpr std::uint32_t kLevelActorFlagHasTag = 1u << 11;
inline constexpr std::uint32_t kLevelActorFlagHasInteractCost = 1u << 12;
inline constexpr std::uint32_t kLevelActorFlagHasPainData = 1u << 13;
inline constexpr std::uint32_t kLevelActorFlagHasPayload = 1u << 14;
inline constexpr std::uint32_t kLevelActorFlagConsumeOnUse = 1u << 15;

// Light `u32 flags` bitfield.
inline constexpr std::uint32_t kLevelLightFlagCastShadows = 1u << 0;
inline constexpr std::uint32_t kLevelLightFlagHasOrbit = 1u << 1;

/// One placed actor as stored in a `.llev` (no GPU / resource handles).
struct LevelActorRecord {
    ELevelActorClass actorClass = ELevelActorClass::StaticMesh;
    EComponentMobility mobility = EComponentMobility::Static;

    bool collisionEnabled = false;
    bool simulatePhysics = false;
    bool enableGravity = true;
    bool hidden = false;

    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotationDegrees{0.0f, 0.0f, 0.0f};
    glm::vec3 scale{1.0f, 1.0f, 1.0f};

    std::string tag;
    std::string materialPath; // `.lmat` path (empty = mesh / default material)
    std::string meshPath;     // imported mesh path (StaticMesh only)
    std::string lightmapId;
    std::string lightmapPath;
    std::uint32_t lightmapResolution = 128;

    // Sphere tessellation (written only when `actorClass == Sphere`).
    std::int32_t sphereSegments = 24;
    std::int32_t sphereRings = 16;

    bool hasSpinYaw = false;
    float spinYaw = 0.0f;

    bool hasBob = false;
    float bobBaseY = 0.0f;
    float bobAmplitude = 0.1f;
    float bobSpeed = 1.0f;

    bool hasFitHeight = false;
    float fitHeight = 0.0f;

    // TriggerVolume / interactables (written when HasInteractCost / HasPayload / ConsumeOnUse).
    int interactCost = 0;
    float interactRadius = 2.0f;
    float damagePerSecond = 12.0f;
    float damageInterval = 0.35f;
    std::string payload;
    bool bConsumeOnUse = false;
};

/// One placed light as stored in a `.llev`.
struct LevelLightRecord {
    ELevelLightClass lightClass = ELevelLightClass::DirectionalLight;
    bool castShadows = true;
    bool hasOrbit = false;

    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 rotationDegrees{0.0f, 0.0f, 0.0f};
    glm::vec3 lightColor{1.0f, 1.0f, 1.0f};

    float intensity = 1.0f;
    float range = 8.0f;
    float sourceAngle = kDefaultLightSourceAngleDegrees;

    float orbitRadius = 1.0f;
    float orbitHeight = 1.0f;
    float orbitHeightAmp = 0.0f;
    float orbitSpeed = 1.0f;
};

/// Camera framing stored in a `.llev` (always present).
struct LevelCameraRecord {
    ECameraMode mode = ECameraMode::Orbit;
    glm::vec3 target{0.0f, 0.0f, 0.0f};
    glm::vec3 eye{0.0f, 0.0f, 0.0f};
    float distance = 5.0f;
    float yaw = 45.0f;
    float pitch = 25.0f;
};

/// In-memory mirror of a `.llev` file: plain data, no engine resources resolved yet.
struct LevelDocument {
    std::string name;
    std::string gameMode;
    std::string environmentPath;
    float environmentExposure = 1.0f;

    LevelCameraRecord camera;
    std::vector<LevelActorRecord> actors;
    std::vector<LevelLightRecord> lights;
};

/// Snapshot a live Level + Camera into a serializable document.
[[nodiscard]] LevelDocument BuildLevelDocument(const Level& level, const Camera& camera);

/// Encode a document as `.llev` bytes.
[[nodiscard]] std::vector<std::uint8_t> SerializeLeonLevel(const LevelDocument& doc);

/// Decode `.llev` bytes; returns false on bad magic / version / truncation.
[[nodiscard]] bool DeserializeLeonLevel(const std::vector<std::uint8_t>& bytes, LevelDocument& out);

/// Write a document to `path` as `.llev`.
[[nodiscard]] bool SaveLeonLevelFile(const std::string& path, const LevelDocument& doc);

/// Read a `.llev` file into `out`.
[[nodiscard]] bool LoadLeonLevelFile(const std::string& path, LevelDocument& out);

/// Resolve a document into the Engine: builds a staging Level, commits on full success only,
/// then hydrates persisted lightmaps relative to `sourcePath`.
[[nodiscard]] bool ApplyLevelDocument(Engine& engine, const LevelDocument& doc,
                                      const std::string& sourcePath,
                                      LevelAnimation* outAnim = nullptr);

/// Resolve a content-relative key (`Materials/M_Floor.lmat`) for a level under `…/Content/Levels/`
/// (or legacy `…/Levels/`).
/// Prefers `<pack>/key`, then global `ResolveAssetPath`.
[[nodiscard]] std::string ResolveLevelAssetPath(const std::string& levelPath,
                                                const std::string& relativeOrKey);

} // namespace leon

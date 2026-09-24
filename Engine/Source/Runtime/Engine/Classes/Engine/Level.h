#pragma once

#include <glm/mat4x4.hpp>

#include <cstddef>
#include <cstdint>
#include "Math/Transform.h"
#include "Level/Light.h"
#include "EnvironmentMap.h"
#include "Material.h"
#include "StaticMesh.h"
#include "Texture2D.h"
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <vector>


/// Unreal-like mobility: Static receives baked lightmaps; Movable uses only dynamic lights.
enum class EComponentMobility : std::uint8_t {
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
struct StaticMeshComponent {
    FTransform transform;
    std::shared_ptr<UStaticMesh> mesh;
    FMaterial material;
    std::vector<FMaterial> materials; // optional per-slot overrides
    /// When true, `material` is used for every submesh (asset/inline overrode MTL).
    bool materialOverride = false;

    /// Optional tag from JSON (`"tag"`). Spawn points use PlayerStart, not tagged meshes.
    std::string tag;
    /// Unreal-like collision enabled — registers a FPhysScene body (Static unless simulating).
    bool collisionEnabled = false;
    /// Unreal-like `bSimulatePhysics` — Dynamic body (implies collision).
    bool simulatePhysics = false;
    /// Unreal-like `bEnableGravity` — applies to simulatePhysics bodies (default true).
    bool enableGravity = true;
    /// When true, skipped by the renderer (Unreal-like BlockingVolume / HiddenInGame).
    bool hidden = false;
    /// When true, renderer uses `modelMatrixOverride` instead of `transform.modelMatrix()`.
    bool bUseModelMatrixOverride = false;
    glm::mat4 modelMatrixOverride{1.0f};

    /// Static = lightmap candidate; Movable = runtime-lit only.
    EComponentMobility mobility = EComponentMobility::Static;
    /// Lightmap texture resolution (power of two; clamped 32–512 on bake).
    int lightmapResolution = 128;
    /// Baked lightmap (Build Lights); sampled with mesh UV0.
    std::shared_ptr<UTexture2D> lightmap;
    /// Stable id for bake file names (`LM_<id>.lm`); survives actor reorder.
    std::string lightmapId;
    /// Relative path to persisted lightmap image (written on Build Lights + Save).
    std::string lightmapPath;

    /// Editor / save provenance (filled by LevelLoader; used by LevelSaver).
    std::string editorClass;  // "Cube", "Sphere", "Plane", "BlockingVolume", "StaticMesh", ...
    std::string meshPath;     // relative mesh path when imported
    std::string materialPath; // relative material JSON path when set
    /// Session-stable editor selection id (0 = unassigned). Not serialized.
    std::uint64_t editorId = 0;
    float spinYaw = 0.0f;
    int sphereSegments = 24;
    int sphereRings = 16;

    /// Optional bob animation (Level JSON `bob`); preserved for save round-trip.
    bool hasBob = false;
    float bobBaseY = 0.0f;
    float bobAmplitude = 0.1f;
    float bobSpeed = 1.0f;

    [[nodiscard]] glm::mat4 EffectiveModelMatrix() const {
        return bUseModelMatrixOverride ? modelMatrixOverride : transform.ModelMatrix();
    }

    [[nodiscard]] bool HasPhysicsBody() const { return collisionEnabled || simulatePhysics; }

    [[nodiscard]] bool UsesLightmap() const {
        return mobility == EComponentMobility::Static && lightmap != nullptr && lightmap->Valid();
    }

    [[nodiscard]] std::size_t subMeshCount() const;
    [[nodiscard]] const FMaterial& materialForSubMesh(std::size_t subMeshIndex) const;
    [[nodiscard]] bool isShadowCaster() const;
};

/// Unreal-like PlayerStart — spawn transform for GameMode-possessed pawns (not a drawable mesh).
struct PlayerStart {
    FTransform transform{};
    /// Session-stable editor selection id (0 = unassigned). Not serialized.
    std::uint64_t editorId = 0;
};

/// Interact / trigger volume (POD). Overlap tested in gameplay from position + interactRadius.
struct TriggerVolume {
    FTransform transform{};
    float interactRadius = 2.f;
    int interactCost = 0;
    std::string payload; // pack-defined e.g. Door, WallBuy:M14, Perk:Jugg
    std::string tag;
    bool bConsumeOnUse = false;
    std::uint64_t editorId = 0;
};

/// Damage volume (POD). AABB from transform.position and abs(scale) * 0.5.
struct PainCausingVolume {
    FTransform transform{}; // position + scale as half-extents box (full size = abs(scale))
    float damagePerSecond = 12.f;
    float damageInterval = 0.35f;
    std::string tag;
    std::uint64_t editorId = 0;
};

/// AI spawn marker (POD — not a drawable mesh).
struct AISpawnPoint {
    FTransform transform{};
    std::string tag;
    std::uint64_t editorId = 0;
};

/// Map content container (Unreal-style Level / ULevel): StaticMeshComponents + lights + env.
/// Distinct from gameplay `World` (spawned Actors). The app owns contents; FSceneRenderer reads them.
class Level {
public:
    StaticMeshComponent& AddStaticMesh(StaticMeshComponent component);
    PlayerStart& AddPlayerStart(PlayerStart start);
    TriggerVolume& AddTriggerVolume(TriggerVolume volume);
    PainCausingVolume& AddPainCausingVolume(PainCausingVolume volume);
    AISpawnPoint& AddAISpawnPoint(AISpawnPoint point);
    void ClearStaticMeshes();
    void ClearPlayerStarts();
    void ClearTriggerVolumes();
    void ClearPainCausingVolumes();
    void ClearAISpawnPoints();
    void ClearLights();
    void Clear();

    [[nodiscard]] const std::vector<StaticMeshComponent>& StaticMeshes() const {
        return staticMeshes_;
    }
    [[nodiscard]] std::vector<StaticMeshComponent>& StaticMeshes() { return staticMeshes_; }

    [[nodiscard]] const std::vector<PlayerStart>& PlayerStarts() const { return playerStarts_; }
    [[nodiscard]] std::vector<PlayerStart>& PlayerStarts() { return playerStarts_; }

    [[nodiscard]] const std::vector<TriggerVolume>& TriggerVolumes() const {
        return triggerVolumes_;
    }
    [[nodiscard]] std::vector<TriggerVolume>& TriggerVolumes() { return triggerVolumes_; }

    [[nodiscard]] const std::vector<PainCausingVolume>& PainCausingVolumes() const {
        return painCausingVolumes_;
    }
    [[nodiscard]] std::vector<PainCausingVolume>& PainCausingVolumes() {
        return painCausingVolumes_;
    }

    [[nodiscard]] const std::vector<AISpawnPoint>& AISpawnPoints() const { return aiSpawnPoints_; }
    [[nodiscard]] std::vector<AISpawnPoint>& AISpawnPoints() { return aiSpawnPoints_; }

    /// First PlayerStart, or nullptr if the level has none.
    [[nodiscard]] const PlayerStart* FindPlayerStart() const;

    /// First static mesh whose tag matches, or npos if none.
    [[nodiscard]] std::size_t FindStaticMeshIndexByTag(std::string_view tag) const;

    [[nodiscard]] const std::vector<DirectionalLight>& DirectionalLights() const {
        return directionalLights_;
    }
    [[nodiscard]] std::vector<DirectionalLight>& DirectionalLights() { return directionalLights_; }

    [[nodiscard]] const std::vector<PointLight>& PointLights() const { return pointLights_; }
    [[nodiscard]] std::vector<PointLight>& PointLights() { return pointLights_; }

    void SetEnvironment(std::shared_ptr<FEnvironmentMap> env) { environment_ = std::move(env); }
    [[nodiscard]] const std::shared_ptr<FEnvironmentMap>& Environment() const { return environment_; }
    void SetEnvironmentExposure(float exposure) { environmentExposure_ = exposure; }
    [[nodiscard]] float EnvironmentExposure() const { return environmentExposure_; }

    /// Relative HDR path from JSON (for editor save round-trip).
    void SetEnvironmentPath(std::string path) { environmentPath_ = std::move(path); }
    [[nodiscard]] const std::string& EnvironmentPath() const { return environmentPath_; }

    void SetName(std::string name) { name_ = std::move(name); }
    [[nodiscard]] const std::string& Name() const { return name_; }
    void SetGameMode(std::string gameMode) { gameMode_ = std::move(gameMode); }
    [[nodiscard]] const std::string& GameMode() const { return gameMode_; }

    static constexpr std::size_t npos = (std::numeric_limits<std::size_t>::max)();

private:
    std::vector<StaticMeshComponent> staticMeshes_;
    std::vector<PlayerStart> playerStarts_;
    std::vector<TriggerVolume> triggerVolumes_;
    std::vector<PainCausingVolume> painCausingVolumes_;
    std::vector<AISpawnPoint> aiSpawnPoints_;
    std::vector<DirectionalLight> directionalLights_{DirectionalLight{}};
    std::vector<PointLight> pointLights_;
    std::shared_ptr<FEnvironmentMap> environment_;
    float environmentExposure_ = 1.0f;
    std::string environmentPath_;
    std::string name_;
    std::string gameMode_;
};


#pragma once

#include <glm/vec3.hpp>

#include <cstddef>
#include <cstdint>
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Level.h"
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>


class UWorld;

/// Unreal-style Actor (no A-prefix): owns a root USceneComponent and optional Level mesh link.
///
/// ## Transforms
/// `location_` / `yawDegrees_` are the gameplay pose written to Level meshes via
/// `SyncTransformToLevel`. The root `USceneComponent` may add `Relative*` offsets on top
/// (`GetComponentTransform`). Prefer setting Actor location/yaw for pawn movement; keep root
/// Relative near identity unless you intentionally offset the visual.
///
/// ## Components
/// Root is always registered. Add member comps with `RegisterComponent`; heap extras with
/// `CreateDefaultSubobject<T>()`. See `UActorComponent` contract.
///
/// ## Actor location vs Level mesh
/// `SetLevelMeshIndex` links this Actor to a Level UStaticMeshComponent for FPhysScene bodies.
/// `SyncTransformToLevel` writes Actor location/yaw into that mesh each gameplay frame
/// (`World::TickGameplayFrame`). Skeletal visuals use SceneComponents (`GetMesh`), not Level
/// meshes.
class AActor {
public:
    virtual ~AActor();

    AActor(const AActor&) = delete;
    AActor& operator=(const AActor&) = delete;
    AActor(AActor&&) = delete;
    AActor& operator=(AActor&&) = delete;

    [[nodiscard]] UWorld* GetWorld() const { return world_; }

    [[nodiscard]] USceneComponent& GetRootComponent() { return rootComponent_; }
    [[nodiscard]] const USceneComponent& GetRootComponent() const { return rootComponent_; }

    [[nodiscard]] const std::vector<UActorComponent*>& GetComponents() const { return components_; }

    /// Register a component that lives on this Actor (member or already owned). Idempotent.
    void RegisterComponent(UActorComponent* component);

    /// Heap-owned component (Unreal CreateDefaultSubobject lite — no name table).
    template <typename T, typename... ArgsType>
    T* CreateDefaultSubobject(ArgsType&&... args) {
        static_assert(std::is_base_of_v<UActorComponent, T>, "T must derive from ActorComponent");
        auto owned = std::make_unique<T>(std::forward<ArgsType>(args)...);
        T* raw = owned.get();
        ownedComponents_.push_back(std::move(owned));
        RegisterComponent(raw);
        return raw;
    }

    void SetLevelMeshIndex(std::size_t index) { levelMeshIndex_ = index; }
    [[nodiscard]] std::size_t LevelMeshIndex() const { return levelMeshIndex_; }

    /// Session-stable id for editor selection (assigned on SpawnActor).
    void SetEditorId(std::uint64_t id) { editorId_ = id; }
    [[nodiscard]] std::uint64_t GetEditorId() const { return editorId_; }

    [[nodiscard]] const glm::vec3& GetActorLocation() const { return location_; }
    [[nodiscard]] float GetActorYaw() const { return yawDegrees_; }

    void SetActorLocation(const glm::vec3& location) {
        location_ = location;
        // Keep root Relative* as identity offset so GetComponentTransform matches Actor pose.
        rootComponent_.RelativeLocation = {0.0f, 0.0f, 0.0f};
    }
    void SetActorYaw(float yawDegrees) {
        yawDegrees_ = yawDegrees;
        rootComponent_.RelativeRotation = {0.0f, 0.0f, 0.0f};
    }

    void SetActorLocationAndRotation(const glm::vec3& location, float yawDegrees = 0.0f) {
        SetActorLocation(location);
        SetActorYaw(yawDegrees);
    }

    /// Unreal-like AActor::IsPendingKill.
    [[nodiscard]] bool IsPendingKill() const { return pendingKill_; }
    [[nodiscard]] bool IsPendingKillPending() const { return IsPendingKill(); }

    /// Mark for removal at end of World::Tick (or immediately via World::Clear).
    virtual void Destroy() {
        if (!pendingKill_) {
            pendingKill_ = true;
        }
    }

    virtual void BeginPlay() {}
    virtual void EndPlay() {}
    virtual void Tick(float /*deltaTime*/) {}

    void BeginPlayComponents();
    void EndPlayComponents();
    void TickComponents(float deltaTime);
    [[nodiscard]] bool HasActorBegunPlay() const { return hasBegunPlay_; }

    /// Copy location + yaw into the linked Level UStaticMeshComponent (no-op if index is invalid).
    virtual void SyncTransformToLevel(ULevel& level) const {
        auto& meshes = level.StaticMeshes();
        if (levelMeshIndex_ >= meshes.size()) {
            return;
        }
        UStaticMeshComponent& obj = meshes[levelMeshIndex_];
        obj.transform.Position = location_;
        obj.transform.RotationDegrees.y = yawDegrees_;
    }

protected:
    AActor() { RegisterComponent(&rootComponent_); }

    [[nodiscard]] glm::vec3& mutableLocation() { return location_; }
    [[nodiscard]] float& mutableYawDegrees() { return yawDegrees_; }

    void UnregisterComponent(UActorComponent* component);

private:
    friend class UWorld;
    friend class UActorComponent;

    USceneComponent rootComponent_{};
    std::vector<UActorComponent*> components_{};
    std::vector<std::unique_ptr<UActorComponent>> ownedComponents_{};
    std::size_t levelMeshIndex_ = ULevel::npos;
    std::uint64_t editorId_ = 0;
    glm::vec3 location_{0.0f};
    float yawDegrees_ = 0.0f;
    UWorld* world_ = nullptr;
    bool pendingKill_ = false;
    bool hasBegunPlay_ = false;
};


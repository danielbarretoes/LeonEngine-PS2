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
/// `Location` / `YawDegrees` are the gameplay pose written to Level meshes via
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
class ENGINE_API AActor {
public:
    virtual ~AActor();

    AActor(const AActor&) = delete;
    AActor& operator=(const AActor&) = delete;
    AActor(AActor&&) = delete;
    AActor& operator=(AActor&&) = delete;

    [[nodiscard]] UWorld* GetWorld() const { return World; }

    [[nodiscard]] USceneComponent& GetRootComponent() { return RootComponent; }
    [[nodiscard]] const USceneComponent& GetRootComponent() const { return RootComponent; }

    [[nodiscard]] const std::vector<UActorComponent*>& GetComponents() const { return Components; }

    /// Register a component that lives on this Actor (member or already owned). Idempotent.
    void RegisterComponent(UActorComponent* Component);

    /// Heap-owned component (Unreal CreateDefaultSubobject lite — no name table).
    template <typename T, typename... ArgsType>
    T* CreateDefaultSubobject(ArgsType&&... Args) {
        static_assert(std::is_base_of_v<UActorComponent, T>, "T must derive from ActorComponent");
        auto Owned = std::make_unique<T>(std::forward<ArgsType>(Args)...);
        T* Raw = Owned.get();
        OwnedComponents.push_back(std::move(Owned));
        RegisterComponent(Raw);
        return Raw;
    }

    void SetLevelMeshIndex(std::size_t Index) { LevelMeshIndex = Index; }
    [[nodiscard]] std::size_t GetLevelMeshIndex() const { return LevelMeshIndex; }

    /// Session-stable id for editor selection (assigned on SpawnActor).
    void SetEditorId(std::uint64_t Id) { EditorId = Id; }
    [[nodiscard]] std::uint64_t GetEditorId() const { return EditorId; }

    [[nodiscard]] const glm::vec3& GetActorLocation() const { return Location; }
    [[nodiscard]] float GetActorYaw() const { return YawDegrees; }

    void SetActorLocation(const glm::vec3& InLocation) {
        Location = InLocation;
        // Keep root Relative* as identity offset so GetComponentTransform matches Actor pose.
        RootComponent.RelativeLocation = {0.0f, 0.0f, 0.0f};
    }
    void SetActorYaw(float InYawDegrees) {
        YawDegrees = InYawDegrees;
        RootComponent.RelativeRotation = {0.0f, 0.0f, 0.0f};
    }

    void SetActorLocationAndRotation(const glm::vec3& InLocation, float InYawDegrees = 0.0f) {
        SetActorLocation(InLocation);
        SetActorYaw(InYawDegrees);
    }

    /// Unreal-like AActor::IsPendingKill.
    [[nodiscard]] bool IsPendingKill() const { return bPendingKill; }
    [[nodiscard]] bool IsPendingKillPending() const { return IsPendingKill(); }

    /// Mark for removal at end of World::Tick (or immediately via World::Clear).
    virtual void Destroy() {
        if (!bPendingKill) {
            bPendingKill = true;
        }
    }

    virtual void BeginPlay() {}
    virtual void EndPlay() {}
    virtual void Tick(float /*deltaTime*/) {}

    void BeginPlayComponents();
    void EndPlayComponents();
    void TickComponents(float DeltaTime);
    [[nodiscard]] bool HasActorBegunPlay() const { return bHasBegunPlay; }

    /// Copy location + yaw into the linked Level UStaticMeshComponent (no-op if index is invalid).
    virtual void SyncTransformToLevel(ULevel& Level) const {
        auto& Meshes = Level.GetStaticMeshes();
        if (LevelMeshIndex >= Meshes.size()) {
            return;
        }
        UStaticMeshComponent& Obj = Meshes[LevelMeshIndex];
        Obj.Transform.Position = Location;
        Obj.Transform.RotationDegrees.y = YawDegrees;
    }

protected:
    AActor() { RegisterComponent(&RootComponent); }

    [[nodiscard]] glm::vec3& MutableLocation() { return Location; }
    [[nodiscard]] float& MutableYawDegrees() { return YawDegrees; }

    void UnregisterComponent(UActorComponent* Component);

private:
    friend class UWorld;
    friend class UActorComponent;

    USceneComponent RootComponent{};
    std::vector<UActorComponent*> Components{};
    std::vector<std::unique_ptr<UActorComponent>> OwnedComponents{};
    std::size_t LevelMeshIndex = ULevel::Npos;
    std::uint64_t EditorId = 0;
    glm::vec3 Location{0.0f};
    float YawDegrees = 0.0f;
    UWorld* World = nullptr;
    bool bPendingKill = false;
    bool bHasBegunPlay = false;
};


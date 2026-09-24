#pragma once

#include <cstdint>
#include "GameFramework/Actor.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Physics/PhysScene.h"
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>


class ACharacter;
class FDebugDraw;
class ULevel;
class FSceneRenderer;

struct FWorldGameplayFrameParams {
    float deltaTime = 0.0f;
    ULevel* level = nullptr;
    FSceneRenderer* renderer = nullptr;
    FDebugDraw* collisionDebugDraw = nullptr;
    FDebugDraw* navMeshDebugDraw = nullptr;
    /// When true, FPhysScene::Step uses these values instead of the first Character's movement.
    bool overridePhysicsStep = false;
    float physicsDamping = 6.0f;
    float physicsWalkBounds = 18.0f;
    float physicsGravity = 24.0f;
    float physicsFloorY = 0.0f;
    float physicsSkin = 0.02f;
};

/// Owns spawned Actors + FPhysScene; ticks them and purges pending kills.
/// Distinct from `Level` (map/visual content ≈ ULevel).
class UWorld {
public:
    explicit UWorld(EPhysicsBackend physicsBackend = DefaultPhysicsBackend())
        : physics_(physicsBackend) {}
    ~UWorld() { Clear(); }

    UWorld(const UWorld&) = delete;
    UWorld& operator=(const UWorld&) = delete;
    UWorld(UWorld&&) = delete;
    UWorld& operator=(UWorld&&) = delete;

    [[nodiscard]] FPhysScene& GetPhysicsScene() { return physics_; }
    [[nodiscard]] const FPhysScene& GetPhysicsScene() const { return physics_; }

    /// Unreal-like UNavigationSystem lite (grid NavMesh for AI pathfinding).
    [[nodiscard]] UNavigationSystem& GetNavigationSystem() { return navigation_; }
    [[nodiscard]] const UNavigationSystem& GetNavigationSystem() const { return navigation_; }

    /// Recreate FPhysScene with another backend (clears bodies). Call before RegisterBodiesFromLevel.
    void SetPhysicsBackend(EPhysicsBackend physicsBackend) { physics_ = FPhysScene(physicsBackend); }

    template <typename T, typename... Args>
    T* SpawnActor(Args&&... args) {
        static_assert(std::is_base_of_v<AActor, T>, "T must derive from Actor");
        auto owned = std::make_unique<T>(std::forward<Args>(args)...);
        T* raw = owned.get();
        raw->world_ = this;
        raw->SetEditorId(++nextEditorId_);
        if (ticking_) {
            // Defer push_back so Tick iterators stay valid.
            pendingSpawns_.push_back(std::move(owned));
        } else {
            actors_.push_back(std::move(owned));
            raw->BeginPlayComponents();
            raw->BeginPlay();
        }
        return raw;
    }

    /// Find live Actor by session-stable editor id (PIE / Outliner).
    [[nodiscard]] AActor* FindActorByEditorId(std::uint64_t editorId) const {
        if (editorId == 0) {
            return nullptr;
        }
        for (const auto& actor : actors_) {
            if (actor && !actor->IsPendingKill() && actor->GetEditorId() == editorId) {
                return actor.get();
            }
        }
        return nullptr;
    }

    void DestroyActor(AActor* actor) {
        if (actor != nullptr && actor->world_ == this) {
            actor->Destroy();
        }
    }

    /// Actor Tick only (UAnimInstance, etc.). Prefer `TickGameplayFrame` for Character worlds.
    void Tick(float deltaTime);

    /// Unreal-like frame: Character move → FPhysScene::Step → overlaps → Actor Tick → sync → draw.
    void TickGameplayFrame(const FWorldGameplayFrameParams& params);

    /// Register StaticMeshComponents that have collision as FPhysScene bodies (clears first).
    void RegisterBodiesFromLevel(const ULevel& level);

    void SubmitSkeletalDraws(FSceneRenderer& renderer) const;

    void Clear();

    [[nodiscard]] std::size_t ActorCount() const { return actors_.size(); }

    template <typename T>
    [[nodiscard]] T* FindFirst() const {
        static_assert(std::is_base_of_v<AActor, T>, "T must derive from Actor");
        for (const auto& actor : actors_) {
            if (actor && !actor->IsPendingKill()) {
                if (T* typed = dynamic_cast<T*>(actor.get())) {
                    return typed;
                }
            }
        }
        return nullptr;
    }

    /// Visit every live Actor of type T.
    template <typename T, typename TFn>
    void ForEach(TFn&& fn) const {
        static_assert(std::is_base_of_v<AActor, T>, "T must derive from Actor");
        for (const auto& actor : actors_) {
            if (actor && !actor->IsPendingKill()) {
                if (T* typed = dynamic_cast<T*>(actor.get())) {
                    fn(*typed);
                }
            }
        }
    }

    /// Visit every live Actor (any type).
    template <typename TFn>
    void ForEachActor(TFn&& fn) const {
        for (const auto& actor : actors_) {
            if (actor && !actor->IsPendingKill()) {
                fn(*actor);
            }
        }
    }

private:
    void flushPendingSpawns();
    void purgePending();
    /// Pairwise Character capsule depenetration (players / AI are not FPhysScene bodies).
    void resolveCharacterOverlaps();

    FPhysScene physics_{};
    UNavigationSystem navigation_{};
    std::vector<std::unique_ptr<AActor>> actors_;
    std::vector<std::unique_ptr<AActor>> pendingSpawns_;
    bool ticking_ = false;
    std::uint64_t nextEditorId_ = 0;
};


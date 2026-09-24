#pragma once

#include <cstdint>
#include <leon/gameplay/Actor.h>
#include <leon/gameplay/NavigationSystem.h>
#include <leon/physics/PhysScene.h>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace leon {

class Character;
class DebugDraw;
class Level;
class Renderer;

struct WorldGameplayFrameParams {
    float deltaTime = 0.0f;
    Level* level = nullptr;
    Renderer* renderer = nullptr;
    DebugDraw* collisionDebugDraw = nullptr;
    DebugDraw* navMeshDebugDraw = nullptr;
    /// When true, PhysScene::Step uses these values instead of the first Character's movement.
    bool overridePhysicsStep = false;
    float physicsDamping = 6.0f;
    float physicsWalkBounds = 18.0f;
    float physicsGravity = 24.0f;
    float physicsFloorY = 0.0f;
    float physicsSkin = 0.02f;
};

/// Owns spawned Actors + PhysScene; ticks them and purges pending kills.
/// Distinct from `Level` (map/visual content ≈ ULevel).
class World {
public:
    explicit World(EPhysicsBackend physicsBackend = DefaultPhysicsBackend())
        : physics_(physicsBackend) {}
    ~World() { Clear(); }

    World(const World&) = delete;
    World& operator=(const World&) = delete;
    World(World&&) = delete;
    World& operator=(World&&) = delete;

    [[nodiscard]] PhysScene& GetPhysicsScene() { return physics_; }
    [[nodiscard]] const PhysScene& GetPhysicsScene() const { return physics_; }

    /// Unreal-like UNavigationSystem lite (grid NavMesh for AI pathfinding).
    [[nodiscard]] NavigationSystem& GetNavigationSystem() { return navigation_; }
    [[nodiscard]] const NavigationSystem& GetNavigationSystem() const { return navigation_; }

    /// Recreate PhysScene with another backend (clears bodies). Call before RegisterBodiesFromLevel.
    void SetPhysicsBackend(EPhysicsBackend physicsBackend) { physics_ = PhysScene(physicsBackend); }

    template <typename T, typename... Args>
    T* SpawnActor(Args&&... args) {
        static_assert(std::is_base_of_v<Actor, T>, "T must derive from leon::Actor");
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
    [[nodiscard]] Actor* FindActorByEditorId(std::uint64_t editorId) const {
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

    void DestroyActor(Actor* actor) {
        if (actor != nullptr && actor->world_ == this) {
            actor->Destroy();
        }
    }

    /// Actor Tick only (AnimInstance, etc.). Prefer `TickGameplayFrame` for Character worlds.
    void Tick(float deltaTime);

    /// Unreal-like frame: Character move → PhysScene::Step → overlaps → Actor Tick → sync → draw.
    void TickGameplayFrame(const WorldGameplayFrameParams& params);

    /// Register StaticMeshComponents that have collision as PhysScene bodies (clears first).
    void RegisterBodiesFromLevel(const Level& level);

    void SubmitSkeletalDraws(Renderer& renderer) const;

    void Clear();

    [[nodiscard]] std::size_t ActorCount() const { return actors_.size(); }

    template <typename T>
    [[nodiscard]] T* FindFirst() const {
        static_assert(std::is_base_of_v<Actor, T>, "T must derive from leon::Actor");
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
        static_assert(std::is_base_of_v<Actor, T>, "T must derive from leon::Actor");
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
    /// Pairwise Character capsule depenetration (players / AI are not PhysScene bodies).
    void resolveCharacterOverlaps();

    PhysScene physics_{};
    NavigationSystem navigation_{};
    std::vector<std::unique_ptr<Actor>> actors_;
    std::vector<std::unique_ptr<Actor>> pendingSpawns_;
    bool ticking_ = false;
    std::uint64_t nextEditorId_ = 0;
};

} // namespace leon

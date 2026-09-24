#pragma once

#include <cstdint>
#include <glm/vec3.hpp>

#include "GameFramework/Controller.h"
#include <vector>


class AActor;
class UNavigationSystem;

/// High-level AAIController mode for packs that do not run a UBehaviorTree.
enum class EAILogicState : std::uint8_t {
    Idle = 0,
    MoveTo = 1,
    Chase = 2,
};

/// Drives a possessed Pawn with simple steering (Unreal-style AAIController).
/// When a UNavigationSystem is set, MoveTo* follows a NavMesh path; otherwise line-of-sight XZ.
class AAIController : public AController {
public:
    void SetWishDirection(const glm::vec3& WishDirXz) { WishDir = WishDirXz; }
    void ClearWishDirection() { WishDir = {}; }

    void SetLogicState(EAILogicState State) { LogicState = State; }
    [[nodiscard]] EAILogicState GetLogicState() const { return LogicState; }

    /// Optional; enables FindPath for MoveToLocation / MoveToActor.
    void SetNavigationSystem(UNavigationSystem* InNavigation) { Navigation = InNavigation; }
    [[nodiscard]] UNavigationSystem* GetNavigationSystem() const { return Navigation; }

    void MoveToLocation(const glm::vec3& WorldPosition);
    /// Chase an Actor each TickAI (repaths periodically when nav is available).
    void MoveToActor(AActor* Actor);
    void StopMovement();

    [[nodiscard]] bool HasMoveTarget() const { return bHasTarget; }
    [[nodiscard]] AActor* GetMoveActor() const { return MoveActor; }
    [[nodiscard]] const glm::vec3& MoveTarget() const { return Target; }
    [[nodiscard]] bool HasPath() const { return !Path.empty(); }
    [[nodiscard]] bool IsFollowingPath() const { return bUsePath && !Path.empty(); }
    [[nodiscard]] const std::vector<glm::vec3>& PathPoints() const { return Path; }

    /// Goal arrive radius (final target). Waypoint arrive stays tight so large values
    /// cannot skip detour corners through a blocker (Euclidean shortcut).
    void SetArriveRadius(float Radius) { ArriveRadius = Radius > 0.0f ? Radius : 0.0f; }
    [[nodiscard]] float GetArriveRadius() const { return ArriveRadius; }

    /// Steer possessed Character (path / target wins over manual wish). Returns wish used.
    glm::vec3 TickAI(float DeltaTime);

private:
    void RebuildPath();
    void ClearPath();
    [[nodiscard]] glm::vec3 SteerToward(const glm::vec3& From, const glm::vec3& To,
                                        float InArriveRadius) const;
    [[nodiscard]] glm::vec3 SteerWithNavFallback(const glm::vec3& From) const;

    glm::vec3 WishDir{0.0f};
    glm::vec3 Target{0.0f};
    AActor* MoveActor = nullptr;
    UNavigationSystem* Navigation = nullptr;
    std::vector<glm::vec3> Path;
    std::size_t PathIndex = 0;
    float PathRebuildCooldown = 0.0f;
    bool bHasTarget = false;
    bool bUsePath = false;
    float ArriveRadius = 0.35f;
    EAILogicState LogicState = EAILogicState::Idle;
};


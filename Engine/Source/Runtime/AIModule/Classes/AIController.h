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
    void SetWishDirection(const glm::vec3& wishDirXZ) { wishDir_ = wishDirXZ; }
    void ClearWishDirection() { wishDir_ = {}; }

    void SetLogicState(EAILogicState state) { logicState_ = state; }
    [[nodiscard]] EAILogicState GetLogicState() const { return logicState_; }

    /// Optional; enables FindPath for MoveToLocation / MoveToActor.
    void SetNavigationSystem(UNavigationSystem* navigation) { navigation_ = navigation; }
    [[nodiscard]] UNavigationSystem* GetNavigationSystem() const { return navigation_; }

    void MoveToLocation(const glm::vec3& worldPosition);
    /// Chase an Actor each TickAI (repaths periodically when nav is available).
    void MoveToActor(AActor* actor);
    void StopMovement();

    [[nodiscard]] bool HasMoveTarget() const { return hasTarget_; }
    [[nodiscard]] AActor* MoveActor() const { return moveActor_; }
    [[nodiscard]] const glm::vec3& MoveTarget() const { return target_; }
    [[nodiscard]] bool HasPath() const { return !path_.empty(); }
    [[nodiscard]] bool IsFollowingPath() const { return usePath_ && !path_.empty(); }
    [[nodiscard]] const std::vector<glm::vec3>& PathPoints() const { return path_; }

    /// Goal arrive radius (final target). Waypoint arrive stays tight so large values
    /// cannot skip detour corners through a blocker (Euclidean shortcut).
    void SetArriveRadius(float radius) { arriveRadius_ = radius > 0.0f ? radius : 0.0f; }
    [[nodiscard]] float ArriveRadius() const { return arriveRadius_; }

    /// Steer possessed Character (path / target wins over manual wish). Returns wish used.
    glm::vec3 TickAI(float deltaTime);

private:
    void RebuildPath();
    void ClearPath();
    [[nodiscard]] glm::vec3 SteerToward(const glm::vec3& from, const glm::vec3& to,
                                        float arriveRadius) const;
    [[nodiscard]] glm::vec3 SteerWithNavFallback(const glm::vec3& from) const;

    glm::vec3 wishDir_{0.0f};
    glm::vec3 target_{0.0f};
    AActor* moveActor_ = nullptr;
    UNavigationSystem* navigation_ = nullptr;
    std::vector<glm::vec3> path_;
    std::size_t pathIndex_ = 0;
    float pathRebuildCooldown_ = 0.0f;
    bool hasTarget_ = false;
    bool usePath_ = false;
    float arriveRadius_ = 0.35f;
    EAILogicState logicState_ = EAILogicState::Idle;
};


#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include "GameFramework/Actor.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "AI/Navigation/NavigationSystem.h"

namespace {

constexpr float kPathRebuildIntervalSeconds = 0.35f;
/// Tight waypoint arrive — must stay well below typical obstacle half-width so path
/// corners are not skipped via straight-line distance through a blocker.
constexpr float kWaypointArriveRadius = 0.45f;

} // namespace

void AAIController::ClearPath() {
    path_.clear();
    pathIndex_ = 0;
    usePath_ = false;
    pathRebuildCooldown_ = 0.0f;
}

void AAIController::RebuildPath() {
    ClearPath();
    ACharacter* character = GetCharacter();
    if (character == nullptr || navigation_ == nullptr || !navigation_->HasNavMesh() ||
        !hasTarget_) {
        return;
    }
    std::vector<glm::vec3> found;
    if (!navigation_->FindPath(character->GetActorLocation(), target_, found) || found.empty()) {
        return;
    }
    path_ = std::move(found);
    pathIndex_ = 0;
    usePath_ = true;
}

glm::vec3 AAIController::SteerToward(const glm::vec3& from, const glm::vec3& to,
                                    float arriveRadius) const {
    const glm::vec3 delta = to - from;
    const glm::vec3 flat{delta.x, 0.0f, delta.z};
    const float distSq = glm::dot(flat, flat);
    const float arrive = arriveRadius * arriveRadius;
    if (distSq <= arrive) {
        return {};
    }
    const float len = std::sqrt(distSq);
    return flat / len;
}

glm::vec3 AAIController::SteerWithNavFallback(const glm::vec3& from) const {
    // Nav is authoritative: never charge the goal in a straight line through blockers.
    if (navigation_ == nullptr || !navigation_->HasNavMesh()) {
        return SteerToward(from, target_, arriveRadius_);
    }
    glm::vec3 onMesh{};
    if (!navigation_->ProjectPointToNavigation(from, onMesh)) {
        return {};
    }
    const glm::vec3 toMesh = SteerToward(from, onMesh, kWaypointArriveRadius);
    if (glm::dot(toMesh, toMesh) > 1.0e-8f) {
        return toMesh;
    }
    glm::vec3 goalNav{};
    if (!navigation_->ProjectPointToNavigation(target_, goalNav)) {
        return {};
    }
    return SteerToward(from, goalNav, arriveRadius_);
}

void AAIController::MoveToLocation(const glm::vec3& worldPosition) {
    moveActor_ = nullptr;
    target_ = worldPosition;
    hasTarget_ = true;
    logicState_ = EAILogicState::MoveTo;
    RebuildPath();
}

void AAIController::MoveToActor(AActor* actor) {
    if (actor == nullptr) {
        StopMovement();
        return;
    }
    const bool sameActor = (moveActor_ == actor);
    moveActor_ = actor;
    hasTarget_ = true;
    logicState_ = EAILogicState::Chase;
    target_ = actor->GetActorLocation();
    // Repath on acquire / when not following; TickAI refreshes on an interval while chasing.
    if (!sameActor || !usePath_) {
        RebuildPath();
    }
}

void AAIController::StopMovement() {
    hasTarget_ = false;
    moveActor_ = nullptr;
    logicState_ = EAILogicState::Idle;
    ClearPath();
}

glm::vec3 AAIController::TickAI(float deltaTime) {
    ACharacter* character = GetCharacter();
    if (character == nullptr) {
        return {};
    }

    if (moveActor_ != nullptr) {
        if (moveActor_->IsPendingKillPending()) {
            moveActor_ = nullptr;
            hasTarget_ = false;
            ClearPath();
        } else {
            target_ = moveActor_->GetActorLocation();
            hasTarget_ = true;
            pathRebuildCooldown_ += deltaTime;
            if (!usePath_ || pathRebuildCooldown_ >= kPathRebuildIntervalSeconds) {
                pathRebuildCooldown_ = 0.0f;
                RebuildPath();
            }
        }
    }

    glm::vec3 wish = wishDir_;
    if (hasTarget_) {
        const glm::vec3 from = character->GetActorLocation();
        if (usePath_ && !path_.empty()) {
            // Advance at most along truly-reached waypoints (tight radius — no Euclidean
            // shortcut through a plate/ramp whose width is smaller than arriveRadius_).
            while (pathIndex_ + 1 < path_.size()) {
                const glm::vec3& wp = path_[pathIndex_];
                const glm::vec3 d = wp - from;
                const float distSq = d.x * d.x + d.z * d.z;
                if (distSq <= kWaypointArriveRadius * kWaypointArriveRadius) {
                    ++pathIndex_;
                } else {
                    break;
                }
            }
            const bool onFinalSegment = pathIndex_ + 1 >= path_.size();
            const glm::vec3& wp = path_[std::min(pathIndex_, path_.size() - 1)];
            if (onFinalSegment) {
                wish = SteerToward(from, target_, arriveRadius_);
            } else {
                wish = SteerToward(from, wp, kWaypointArriveRadius);
            }
        } else {
            wish = SteerWithNavFallback(from);
        }
    }

    character->AddMovementInput(wish);
    return wish;
}


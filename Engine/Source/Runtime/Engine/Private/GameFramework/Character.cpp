#include <glm/geometric.hpp>

#include <algorithm>
#include <cmath>
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "SceneRenderer.h"
#include <numbers>
#include <vector>

namespace {

float yawDegreesFromMoveXZ(const glm::vec3& moveXZ) {
    constexpr float kRadToDeg = 180.0f / std::numbers::pi_v<float>;
    return std::atan2(moveXZ.x, moveXZ.z) * kRadToDeg;
}

float shortestYawDelta(float fromDeg, float toDeg) {
    float delta = std::fmod(toDeg - fromDeg + 540.0f, 360.0f) - 180.0f;
    if (delta <= -180.0f) {
        delta += 360.0f;
    }
    return delta;
}

} // namespace

Character::Character() {
    RegisterComponent(&mesh_);
    (void)mesh_.AttachToComponent(&GetRootComponent());
}

void Character::SetHealth(float health) {
    health_ = std::clamp(health, 0.0f, maxHealth_);
    bAlive_ = health_ > 0.0f;
}

void Character::SetMaxHealth(float maxHealth) {
    maxHealth_ = std::max(0.0f, maxHealth);
    if (health_ > maxHealth_) {
        health_ = maxHealth_;
    }
}

float Character::TakeDamage(float DamageAmount) {
    if (!bAlive_ || DamageAmount <= 0.0f) {
        return 0.0f;
    }
    const float applied = std::min(health_, DamageAmount);
    health_ = std::max(0.0f, health_ - DamageAmount);
    if (health_ <= 0.0f) {
        Die();
    }
    return applied;
}

void Character::Die() {
    health_ = 0.0f;
    bAlive_ = false;
}

void Character::Revive(float NewHealth) {
    health_ = std::clamp(NewHealth, 0.0f, maxHealth_);
    bAlive_ = true;
}

void Character::Reset(const glm::vec3& location, float yawDegrees) {
    SetActorLocationAndRotation(location, yawDegrees);
    wishDir_ = {};
    velocityY_ = 0.0f;
    SetMovementMode(EMovementMode::Walking);
    jumpRequested_ = false;
    justLanded_ = false;
    yawInitialized_ = false;
    jumpsRemaining_ = std::max(0, movement_.MaxJumpCount - 1);
    currentFloor_ = {};
    health_ = maxHealth_;
    bAlive_ = true;
}

void Character::ApplyReplicatedState(const glm::vec3& location, float yawDegrees, float velocityY,
                                     bool grounded) {
    SetActorLocationAndRotation(location, yawDegrees);
    wishDir_ = {};
    velocityY_ = velocityY;
    SetMovementMode(grounded ? EMovementMode::Walking : EMovementMode::Falling);
    jumpRequested_ = false;
    yawInitialized_ = true;
}

void Character::SetMovementMode(EMovementMode newMode) {
    if (newMode == EMovementMode::None) {
        newMode = EMovementMode::Walking;
    }
    movementMode_ = newMode;
}

void Character::AddMovementInput(const glm::vec3& wishDirXZ) {
    wishDir_ = wishDirXZ;
}

void Character::SetAnimBlendInput(float speedAlpha) {
    animBlendInput_ = std::clamp(speedAlpha, 0.0f, 1.0f);
    mesh_.GetAnimInstance().SetBlendSpaceInput(animBlendInput_);
}

bool Character::ConsumeJustLanded() {
    const bool landed = justLanded_;
    justLanded_ = false;
    return landed;
}

void Character::Jump() {
    jumpRequested_ = true;
}

void Character::FaceRotation(float yawDegrees, float deltaTime) {
    applyYaw(yawDegrees, deltaTime);
}

bool Character::IsWalkable(const FHitResult& hit) const {
    if (!hit.bBlockingHit) {
        return false;
    }
    return hit.ImpactNormal.y >= movement_.WalkableFloorZ;
}

void Character::FindFloor(FPhysScene& physScene, FindFloorResult& outFloor, float traceDistance,
                          FDebugDraw* debugDraw) const {
    outFloor = {};
    const float distance = std::max(traceDistance, movement_.Skin);
    const glm::vec3 feet = GetActorLocation();
    // Sphere rests on the feet (center = feet + radius up).
    const glm::vec3 sphereCenter = feet + glm::vec3{0.0f, capsule_.radius, 0.0f};
    const glm::vec3 traceStart = sphereCenter + glm::vec3{0.0f, movement_.Skin, 0.0f};
    const glm::vec3 traceEnd = sphereCenter - glm::vec3{0.0f, distance, 0.0f};

    FCollisionQueryParams query{};
    query.SkipLevelMeshIndex = LevelMeshIndex();
    query.bTraceFloorPlane = true;
    query.FloorY = movement_.FloorY;
    query.DrawDebugType =
        debugDraw != nullptr ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

    FHitResult hit{};
    const bool hitFloor =
        physScene.SphereTraceSingleByChannel(hit, traceStart, traceEnd, capsule_.radius,
                                             ECollisionChannel::Visibility, query, debugDraw);
    if (!hitFloor) {
        return;
    }

    outFloor.bBlockingHit = true;
    outFloor.Hit = hit;
    outFloor.bWalkableFloor = IsWalkable(hit);
    outFloor.FloorDist = std::max(0.0f, feet.y - hit.ImpactPoint.y);
}

void Character::applyYaw(float targetYawDegrees, float deltaTime) {
    if (!yawInitialized_) {
        mutableYawDegrees() = targetYawDegrees;
        yawInitialized_ = true;
        return;
    }
    const float delta = shortestYawDelta(GetActorYaw(), targetYawDegrees);
    const float t = 1.0f - std::exp(-movement_.TurnSharpness * deltaTime);
    mutableYawDegrees() += delta * t;
}

float Character::capsuleHalfHeight() const {
    return std::max(0.0f, capsule_.height * 0.5f - capsule_.radius);
}

glm::vec3 Character::capsuleCenterFromFeet(const glm::vec3& feet) const {
    return feet + glm::vec3{0.0f, capsule_.height * 0.5f, 0.0f};
}

bool Character::blocksHorizontalMove(const FHitResult& hit) const {
    if (!hit.bBlockingHit || hit.bFloorPlane) {
        return false;
    }
    // Walkable tops must not stop XZ travel (standing on / stepping onto AABB).
    if (IsWalkable(hit) || hit.ImpactNormal.y > 0.5f) {
        return false;
    }
    return true;
}

glm::vec3 Character::computeSlideVector(const glm::vec3& delta, const glm::vec3& impactNormal) {
    glm::vec3 n{impactNormal.x, 0.0f, impactNormal.z};
    const float nLen = glm::length(n);
    if (nLen < 1.0e-4f) {
        return glm::vec3{0.0f};
    }
    n /= nLen;
    glm::vec3 slide = delta - n * glm::dot(delta, n);
    slide.y = 0.0f;
    return slide;
}

bool Character::safeMoveUpdatedComponent(FPhysScene& physScene, const glm::vec3& delta,
                                         FHitResult* outHit, FDebugDraw* debugDraw) {
    glm::vec3& feet = mutableLocation();
    const float deltaLen = glm::length(delta);
    if (deltaLen < 1.0e-6f) {
        return true;
    }

    const glm::vec3 startCenter = capsuleCenterFromFeet(feet);
    const glm::vec3 endCenter = startCenter + delta;
    const float halfH = capsuleHalfHeight();

    FCollisionQueryParams query{};
    query.SkipLevelMeshIndex = LevelMeshIndex();
    query.bTraceFloorPlane = false;
    query.DrawDebugType =
        debugDraw != nullptr ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

    std::vector<FHitResult> hits;
    (void)physScene.CapsuleTraceMultiByChannel(hits, startCenter, endCenter, capsule_.radius, halfH,
                                               ECollisionChannel::Visibility, query, debugDraw);

    const FHitResult* block = nullptr;
    for (const FHitResult& hit : hits) {
        if (blocksHorizontalMove(hit)) {
            block = &hit;
            break;
        }
    }

    if (block == nullptr) {
        feet += delta;
        ClampPositionXZ(feet, movement_.WalkBounds);
        if (outHit != nullptr) {
            *outHit = {};
        }
        return true;
    }

    float t = block->Time;
    t = std::max(0.0f, t - (movement_.Skin / deltaLen));
    feet += delta * t;
    // Nudge out of the wall so the next iteration does not re-hit at t=0.
    glm::vec3 n{block->ImpactNormal.x, 0.0f, block->ImpactNormal.z};
    const float nLen = glm::length(n);
    if (nLen > 1.0e-4f) {
        n /= nLen;
        feet += n * movement_.Skin;
    }
    // Sweep stops before overlap; ResolveCapsuleSides push never fires — shove from the hit.
    (void)physScene.ApplyCapsuleSweepPush(block->LevelMeshIndex, {wishDir_.x, wishDir_.z},
                                          block->ImpactNormal, movement_.PushStrength,
                                          movement_.WalkBounds);
    ClampPositionXZ(feet, movement_.WalkBounds);
    if (outHit != nullptr) {
        *outHit = *block;
    }
    return false;
}

void Character::resolveSides(FPhysScene& physScene, bool applyPush) {
    FCapsuleContactParams params{};
    params.pushStrength = movement_.PushStrength;
    params.stepUp = movement_.MaxStepHeight;
    params.skin = movement_.Skin;
    params.walkBounds = movement_.WalkBounds;
    physScene.ResolveCapsuleSides(capsule_, mutableLocation(), {wishDir_.x, wishDir_.z}, params,
                                  LevelMeshIndex(), applyPush);
}

bool Character::tryStepUp(FPhysScene& physScene, const glm::vec3& forwardDelta,
                          FDebugDraw* debugDraw) {
    if (!IsMovingOnGround() || movement_.MaxStepHeight <= 1.0e-4f) {
        return false;
    }
    glm::vec3 fwd = forwardDelta;
    fwd.y = 0.0f;
    if (glm::length(fwd) < 1.0e-5f) {
        return false;
    }

    glm::vec3& feet = mutableLocation();
    const glm::vec3 startFeet = feet;
    const float halfH = capsuleHalfHeight();

    FCollisionQueryParams query{};
    query.SkipLevelMeshIndex = LevelMeshIndex();
    query.bTraceFloorPlane = false;
    query.DrawDebugType =
        debugDraw != nullptr ? EDrawDebugTrace::ForOneFrame : EDrawDebugTrace::None;

    // 1) Raise by MaxStepHeight; only a true ceiling (downward normal) aborts.
    const glm::vec3 upStart = capsuleCenterFromFeet(feet);
    const glm::vec3 upEnd = upStart + glm::vec3{0.0f, movement_.MaxStepHeight, 0.0f};
    std::vector<FHitResult> upHits;
    (void)physScene.CapsuleTraceMultiByChannel(upHits, upStart, upEnd, capsule_.radius, halfH,
                                               ECollisionChannel::Visibility, query, debugDraw);
    for (const FHitResult& upHit : upHits) {
        if (upHit.ImpactNormal.y < -0.5f) {
            return false;
        }
    }
    feet.y = startFeet.y + movement_.MaxStepHeight;

    // 2) Forward onto the ledge while elevated. One frame of leftover is often << radius;
    // probe at least ~half-radius so QuerySupportY can see the top.
    const float fwdLen = glm::length(fwd);
    const float minFwd = std::max(capsule_.radius * 0.5f, movement_.Skin * 4.0f);
    if (fwdLen > 1.0e-5f && fwdLen < minFwd) {
        fwd *= (minFwd / fwdLen);
    }
    FHitResult fwdHit{};
    const bool cleared = safeMoveUpdatedComponent(physScene, fwd, &fwdHit, debugDraw);
    if (!cleared && fwdHit.Time < 0.15f) {
        feet = startFeet;
        return false;
    }

    // 3) Land on a raised walkable support within MaxStepHeight (not the floor below).
    FindFloorResult floor{};
    FindFloor(physScene, floor, movement_.MaxStepHeight + (movement_.Skin * 4.0f), debugDraw);
    if (!floor.bWalkableFloor) {
        feet = startFeet;
        return false;
    }
    const float heightGain = floor.Hit.ImpactPoint.y - startFeet.y;
    if (heightGain < movement_.Skin || heightGain > movement_.MaxStepHeight + movement_.Skin) {
        feet = startFeet;
        return false;
    }
    // Sphere FindFloor can report a phantom shelf in front of an AABB; require real support.
    const float support =
        physScene.QuerySupportY(capsule_, feet, movement_.FloorY, movement_.MaxStepHeight,
                                movement_.Skin, LevelMeshIndex());
    if (support < startFeet.y + movement_.Skin) {
        feet = startFeet;
        return false;
    }

    feet.y = std::max(floor.Hit.ImpactPoint.y, support);
    if (feet.y - startFeet.y > movement_.MaxStepHeight + movement_.Skin) {
        feet = startFeet;
        return false;
    }
    velocityY_ = 0.0f;
    SetMovementMode(EMovementMode::Walking);
    currentFloor_ = floor;
    ClampPositionXZ(feet, movement_.WalkBounds);
    return true;
}

void Character::moveHorizontal(FPhysScene& physScene, float deltaTime, FDebugDraw* debugDraw) {
    const float len = glm::length(wishDir_);
    if (len <= 1.0e-4f) {
        resolveSides(physScene, true);
        return;
    }

    const glm::vec3 dir = wishDir_ / len;
    if (bOrientRotationToMovement) {
        applyYaw(yawDegreesFromMoveXZ(dir) + movement_.ModelYawOffsetDegrees, deltaTime);
    }

    // Flow: SafeMove → step-up (if Walking + blocked) → slide → ResolveCapsuleSides.
    // Falling uses AirControl fraction of MaxWalkSpeed (Unreal AirControl lite).
    float speedScale = 1.0f;
    if (IsFalling()) {
        speedScale = std::clamp(movement_.AirControl, 0.0f, 1.0f);
    }
    glm::vec3 remaining = dir * (movement_.MaxWalkSpeed * speedScale * deltaTime);
    remaining.y = 0.0f;
    constexpr int kMaxSlideIterations = 2;
    for (int i = 0; i < kMaxSlideIterations; ++i) {
        if (glm::length(remaining) < 1.0e-5f) {
            break;
        }
        FHitResult hit{};
        if (safeMoveUpdatedComponent(physScene, remaining, &hit, debugDraw)) {
            break;
        }
        const float used = std::clamp(hit.Time, 0.0f, 1.0f);
        glm::vec3 leftover = remaining * (1.0f - used);
        leftover.y = 0.0f;
        if (tryStepUp(physScene, leftover, debugDraw)) {
            break;
        }
        leftover = computeSlideVector(leftover, hit.ImpactNormal);
        if (glm::dot(leftover, leftover) < 1.0e-8f) {
            break;
        }
        remaining = leftover;
    }

    resolveSides(physScene, true);
}

void Character::integrateVertical(FPhysScene& physScene, float deltaTime, FDebugDraw* debugDraw) {
    const bool wasGrounded = IsMovingOnGround();
    if (jumpRequested_) {
        const bool canGroundJump = wasGrounded;
        const bool canAirJump = !wasGrounded && jumpsRemaining_ > 0;
        if (canGroundJump || canAirJump) {
            velocityY_ = movement_.JumpZVelocity;
            SetMovementMode(EMovementMode::Falling);
            if (canGroundJump) {
                jumpsRemaining_ = std::max(0, movement_.MaxJumpCount - 1);
            } else {
                --jumpsRemaining_;
            }
            if (auto* characterAnim =
                    dynamic_cast<UCharacterAnimInstance*>(&mesh_.GetAnimInstance())) {
                characterAnim->NotifyJumped();
            }
        }
    }
    jumpRequested_ = false;

    velocityY_ -= movement_.Gravity * deltaTime;
    mutableLocation().y += velocityY_ * deltaTime;

    // Flow: FindFloor → IsWalkable → snap (Walking) or reject steep (Falling, no tunnel)
    const float landWindow = std::max(movement_.MaxStepHeight + movement_.Skin,
                                      (std::abs(velocityY_) * deltaTime) + (movement_.Skin * 4.0f));
    FindFloor(physScene, currentFloor_, landWindow, debugDraw);

    if (velocityY_ <= 0.0f && currentFloor_.bBlockingHit) {
        const float surfaceY = currentFloor_.Hit.ImpactPoint.y;
        if (currentFloor_.bWalkableFloor) {
            mutableLocation().y = surfaceY;
            velocityY_ = 0.0f;
            SetMovementMode(EMovementMode::Walking);
            jumpsRemaining_ = std::max(0, movement_.MaxJumpCount - 1);
            if (!wasGrounded) {
                justLanded_ = true;
            }
        } else {
            // Unreal: unwalkable floor (steep) — stay Falling, do not sink through.
            if (mutableLocation().y < surfaceY) {
                mutableLocation().y = surfaceY;
            }
            if (currentFloor_.FloorDist <= (movement_.Skin * 4.0f) && velocityY_ < 0.0f) {
                velocityY_ = 0.0f;
            }
            SetMovementMode(EMovementMode::Falling);
        }
    } else {
        SetMovementMode(EMovementMode::Falling);
    }
}

void Character::PerformMovement(FPhysScene& physScene, float deltaTime, FDebugDraw* debugDraw) {
    moveHorizontal(physScene, deltaTime, debugDraw);
    integrateVertical(physScene, deltaTime, debugDraw);
    resolveSides(physScene, false);
}

void Character::TickCharacterMovement(float deltaTime, FDebugDraw* debugDraw) {
    World* world = GetWorld();
    if (world == nullptr) {
        return;
    }
    PerformMovement(world->GetPhysicsScene(), deltaTime, debugDraw);
}

void Character::ResolveOverlaps(FPhysScene& physScene) {
    resolveSides(physScene, false);
}

void Character::ResolveOverlaps() {
    World* world = GetWorld();
    if (world == nullptr) {
        return;
    }
    ResolveOverlaps(world->GetPhysicsScene());
}

void Character::ResolvePawnOverlap(Character& other) {
    if (this == &other) {
        return;
    }

    glm::vec3& a = mutableLocation();
    glm::vec3& b = other.mutableLocation();
    const float aTop = a.y + capsule_.height;
    const float bTop = b.y + other.capsule_.height;
    if (aTop < b.y || bTop < a.y) {
        return;
    }

    glm::vec2 delta{a.x - b.x, a.z - b.z};
    float dist = glm::length(delta);
    const float minDist = capsule_.radius + other.capsule_.radius;
    if (dist >= minDist - 1.0e-5f) {
        return;
    }

    glm::vec2 normal{};
    if (dist < 1.0e-4f) {
        // Deterministic axis when centers coincide (avoid NaN / jitter).
        normal =
            (GetEditorId() <= other.GetEditorId()) ? glm::vec2{1.0f, 0.0f} : glm::vec2{-1.0f, 0.0f};
        dist = 0.0f;
    } else {
        normal = delta / dist;
    }

    const float penetration = minDist - dist;
    const float half = penetration * 0.5f;
    a.x += normal.x * half;
    a.z += normal.y * half;
    b.x -= normal.x * half;
    b.z -= normal.y * half;
    ClampPositionXZ(a, movement_.WalkBounds);
    ClampPositionXZ(b, other.movement_.WalkBounds);
}

void Character::Tick(float deltaTime) {
    if (auto* characterAnim = dynamic_cast<UCharacterAnimInstance*>(&mesh_.GetAnimInstance())) {
        characterAnim->SetMovementState(IsFalling(), velocityY_, ConsumeJustLanded());
    } else {
        (void)ConsumeJustLanded();
    }
    mesh_.TickComponent(deltaTime);
}

void Character::SubmitMeshDraw(FSceneRenderer& renderer) const {
    mesh_.SubmitDraw(renderer);
}


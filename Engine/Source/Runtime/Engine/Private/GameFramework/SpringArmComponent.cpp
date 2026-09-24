#include "GameFramework/SpringArmComponent.h"

#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>

#include <cmath>
#include "Debug/DebugDraw.h"
#include "Engine/World.h"
#include "Physics/PhysScene.h"

namespace {

[[nodiscard]] PhysScene* ResolvePhysScene(Actor* owner, PhysScene* explicitScene) {
    if (explicitScene != nullptr) {
        return explicitScene;
    }
    if (owner == nullptr) {
        return nullptr;
    }
    World* world = owner->GetWorld();
    return world != nullptr ? &world->GetPhysicsScene() : nullptr;
}

} // namespace

glm::vec3 SpringArmComponent::GetBoomDirection(float yawDegrees, float pitchDegrees) {
    const float yawRad = yawDegrees * (glm::pi<float>() / 180.0f);
    const float pitchRad = pitchDegrees * (glm::pi<float>() / 180.0f);
    const glm::vec3 dir{
        std::cos(pitchRad) * std::cos(yawRad),
        std::sin(pitchRad),
        std::cos(pitchRad) * std::sin(yawRad),
    };
    const float len = glm::length(dir);
    if (len < 1.0e-6f) {
        return glm::vec3{0.0f, 0.0f, 1.0f};
    }
    return dir / len;
}

glm::vec3 SpringArmComponent::GetTargetLocation(const glm::vec3& actorLocation) const {
    constexpr float kDegToRad = glm::pi<float>() / 180.0f;
    const float yawRad = BoomYawDegrees * kDegToRad;
    // Same right basis as yawRelativeMoveXZ.
    const glm::vec3 right{std::sin(yawRad), 0.0f, -std::cos(yawRad)};
    return actorLocation + glm::vec3{0.0f, SocketOffsetZ, 0.0f} + (right * SocketOffsetX);
}

void SpringArmComponent::SnapLagState(const glm::vec3& actorLocation) {
    laggedTarget_ = GetTargetLocation(actorLocation);
    laggedYawDegrees_ = BoomYawDegrees;
    laggedPitchDegrees_ = BoomPitchDegrees;
    laggedArmLength_ = TargetArmLength;
    lagInitialized_ = true;
}

float SpringArmComponent::GetLookFacingYawDegrees() const {
    constexpr float kDegToRad = glm::pi<float>() / 180.0f;
    constexpr float kRadToDeg = 180.0f / glm::pi<float>();
    const float yawRad = BoomYawDegrees * kDegToRad;
    // Same forward as yawRelativeMoveXZ / Camera orbit look on XZ.
    return std::atan2(-std::cos(yawRad), -std::sin(yawRad)) * kRadToDeg;
}

float SpringArmComponent::ExpSmoothAlpha(float speed, float deltaTime) {
    if (speed <= 0.0f || deltaTime <= 0.0f) {
        return 1.0f;
    }
    return 1.0f - std::exp(-speed * deltaTime);
}

float SpringArmComponent::LerpAngleDegrees(float fromDegrees, float toDegrees, float alpha) {
    float delta = std::fmod(toDegrees - fromDegrees + 540.0f, 360.0f) - 180.0f;
    return fromDegrees + (delta * alpha);
}

void SpringArmComponent::UpdateLag(float deltaTime, const glm::vec3& actorLocation) {
    const glm::vec3 desiredTarget = GetTargetLocation(actorLocation);
    if (!lagInitialized_) {
        laggedTarget_ = desiredTarget;
        laggedYawDegrees_ = BoomYawDegrees;
        laggedPitchDegrees_ = BoomPitchDegrees;
        laggedArmLength_ = TargetArmLength;
        lagInitialized_ = true;
        return;
    }

    const float posAlpha = bEnableCameraLag ? ExpSmoothAlpha(CameraLagSpeed, deltaTime) : 1.0f;
    laggedTarget_ = glm::mix(laggedTarget_, desiredTarget, posAlpha);

    const float rotAlpha =
        bEnableCameraRotationLag ? ExpSmoothAlpha(CameraRotationLagSpeed, deltaTime) : 1.0f;
    laggedYawDegrees_ = LerpAngleDegrees(laggedYawDegrees_, BoomYawDegrees, rotAlpha);
    laggedPitchDegrees_ = glm::mix(laggedPitchDegrees_, BoomPitchDegrees, rotAlpha);

    const float armAlpha = ExpSmoothAlpha(ArmLengthLagSpeed, deltaTime);
    laggedArmLength_ = glm::mix(laggedArmLength_, TargetArmLength, armAlpha);
    laggedArmLength_ = std::clamp(laggedArmLength_, ArmLengthMin, ArmLengthMax);
}

float SpringArmComponent::ProbeArmLength(PhysScene& physScene, const glm::vec3& target,
                                         float yawDegrees, float pitchDegrees,
                                         float desiredLength, FDebugDraw* debugDraw) const {
    const float length = std::clamp(desiredLength, ArmLengthMin, ArmLengthMax);
    if (ProbeSize <= 0.0f || length <= ArmLengthMin + 1.0e-4f) {
        return length;
    }

    const glm::vec3 boomDir = GetBoomDirection(yawDegrees, pitchDegrees);
    const glm::vec3 end = target + boomDir * length;

    CollisionQueryParams params{};
    params.bTraceFloorPlane = false;
    if (debugDraw != nullptr) {
        params.DrawDebugType = EDrawDebugTrace::ForOneFrame;
    }

    HitResult hit{};
    if (!physScene.SphereTraceSingleByChannel(hit, target, end, ProbeSize, ProbeChannel, params,
                                              debugDraw) ||
        !hit.bBlockingHit) {
        return length;
    }

    // Pull in slightly past the sweep center so the near clip stays clear of the surface.
    const float cleared = hit.Distance - CollisionProbeOffset;
    return std::clamp(cleared, ArmLengthMin, length);
}

void SpringArmComponent::ApplyToCamera(Camera& camera, const glm::vec3& actorLocation,
                                       float deltaTime, PhysScene* physScene,
                                       FDebugDraw* debugDraw) {
    UpdateLag(deltaTime, actorLocation);

    float armLength = laggedArmLength_;
    PhysScene* phys = ResolvePhysScene(GetOwner(), physScene);
    if (bDoCollisionTest && phys != nullptr) {
        // Flow: lag desired length → sphere probe target→eye → snap in on hit (no lerp through walls)
        const float probed =
            ProbeArmLength(*phys, laggedTarget_, laggedYawDegrees_, laggedPitchDegrees_, armLength,
                           debugDraw);
        if (probed < armLength) {
            armLength = probed;
            laggedArmLength_ = probed;
        }
    }

    camera.SetMode(ECameraMode::Orbit);
    camera.SetTarget(laggedTarget_);
    camera.SetDistance(armLength);
    camera.SetYawPitch(laggedYawDegrees_, laggedPitchDegrees_);
}

void SpringArmComponent::ApplyToCamera(Camera& camera, float deltaTime, FDebugDraw* debugDraw) {
    const glm::vec3 actorLocation =
        GetOwner() != nullptr ? GetOwner()->GetActorLocation() : GetComponentLocation();
    ApplyToCamera(camera, actorLocation, deltaTime, nullptr, debugDraw);
}


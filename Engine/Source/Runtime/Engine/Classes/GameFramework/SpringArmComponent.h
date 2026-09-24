#pragma once

#include <glm/common.hpp>
#include <glm/vec3.hpp>

#include <algorithm>
#include <cmath>
#include "Camera/CameraComponent.h"
#include "GameFramework/Input.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "CollisionQuery.h"


class FDebugDraw;
class FPhysScene;

/// Unreal-like Spring Arm / Camera Boom (USceneComponent) with optional camera lag,
/// rotation lag, smoothed arm length, and collision probe (sphere sweep).
class USpringArmComponent : public USceneComponent {
public:
    /// Desired boom length (scroll edits this; lag follows toward it).
    float TargetArmLength = 4.0f;
    float ArmLengthMin = 1.5f;
    float ArmLengthMax = 20.0f;
    float SocketOffsetZ = 1.0f;
    /// Unreal-like SocketOffset.X — positive = right of pawn along boom right (over-shoulder).
    float SocketOffsetX = 0.0f;

    /// Desired boom orientation (mouse look edits these immediately).
    float BoomYawDegrees = 0.0f;
    float BoomPitchDegrees = 15.0f;
    float PitchMin = -60.0f;
    float PitchMax = 70.0f;

    /// Unreal-style follow lag (higher speed = snappier).
    bool bEnableCameraLag = true;
    float CameraLagSpeed = 10.0f;
    bool bEnableCameraRotationLag = true;
    float CameraRotationLagSpeed = 14.0f;
    /// Smooth zoom toward TargetArmLength.
    float ArmLengthLagSpeed = 10.0f;

    /// Unreal `bDoCollisionTest` — sphere-sweep target → camera against FPhysScene.
    bool bDoCollisionTest = true;
    /// Sphere probe radius (meters). Unreal ProbeSize ≈ 12uu → ~0.12m.
    float ProbeSize = 0.15f;
    /// Extra pull-in after a hit so the near plane stays clear of geometry.
    float CollisionProbeOffset = 0.05f;
    ECollisionChannel ProbeChannel = ECollisionChannel::WorldStatic;

    void AddYawInput(float deltaDegrees) { BoomYawDegrees += deltaDegrees; }

    void AddPitchInput(float deltaDegrees) {
        BoomPitchDegrees = std::clamp(BoomPitchDegrees + deltaDegrees, PitchMin, PitchMax);
    }

    /// Positive delta lengthens the boom (zoom out). Clamped to ArmLengthMin/Max.
    void AddArmLengthInput(float deltaLength) {
        TargetArmLength = std::clamp(TargetArmLength + deltaLength, ArmLengthMin, ArmLengthMax);
    }

    void ClampPitch() { BoomPitchDegrees = std::clamp(BoomPitchDegrees, PitchMin, PitchMax); }

    [[nodiscard]] glm::vec3 GetTargetLocation(const glm::vec3& actorLocation) const;

    /// Snap lagged state to desired (call on possess / level enter).
    void SnapLagState(const glm::vec3& actorLocation);

    /// Movement uses *desired* boom yaw so controls stay responsive while the view lags.
    [[nodiscard]] glm::vec3 GetMoveDirectionXZ(const FMoveAxes2D& axes) const {
        return yawRelativeMoveXZ(BoomYawDegrees, axes);
    }

    /// World yaw for a pawn facing the same XZ direction the orbit camera looks
    /// (matches `yawRelativeMoveXZ` forward / crosshair aim on the ground plane).
    [[nodiscard]] float GetLookFacingYawDegrees() const;

    /// Advance lag, optional collision probe, and push the Engine orbit camera.
    /// If `physScene` is null, uses `GetOwner()->GetWorld()->GetPhysicsScene()` when available.
    void ApplyToCamera(UCameraComponent& camera, const glm::vec3& actorLocation, float deltaTime,
                       FPhysScene* physScene = nullptr, FDebugDraw* debugDraw = nullptr);

    /// Prefer when attached under an Actor root: uses owner location + world FPhysScene.
    void ApplyToCamera(UCameraComponent& camera, float deltaTime, FDebugDraw* debugDraw = nullptr);

    /// Unit boom direction matching `Camera` orbit eye offset (target → camera).
    [[nodiscard]] static glm::vec3 GetBoomDirection(float yawDegrees, float pitchDegrees);

    /// Sphere-sweep arm length; returns clamped length (ArmLengthMin..desiredLength).
    [[nodiscard]] float ProbeArmLength(FPhysScene& physScene, const glm::vec3& target,
                                       float yawDegrees, float pitchDegrees, float desiredLength,
                                       FDebugDraw* debugDraw = nullptr) const;

private:
    [[nodiscard]] static float ExpSmoothAlpha(float speed, float deltaTime);
    [[nodiscard]] static float LerpAngleDegrees(float fromDegrees, float toDegrees, float alpha);
    void UpdateLag(float deltaTime, const glm::vec3& actorLocation);

    glm::vec3 laggedTarget_{0.0f};
    float laggedYawDegrees_ = 0.0f;
    float laggedPitchDegrees_ = 15.0f;
    float laggedArmLength_ = 4.0f;
    bool lagInitialized_ = false;
};


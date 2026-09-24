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
class ENGINE_API USpringArmComponent : public USceneComponent {
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

    void AddYawInput(float DeltaDegrees) { BoomYawDegrees += DeltaDegrees; }

    void AddPitchInput(float DeltaDegrees) {
        BoomPitchDegrees = std::clamp(BoomPitchDegrees + DeltaDegrees, PitchMin, PitchMax);
    }

    /// Positive delta lengthens the boom (zoom out). Clamped to ArmLengthMin/Max.
    void AddArmLengthInput(float DeltaLength) {
        TargetArmLength = std::clamp(TargetArmLength + DeltaLength, ArmLengthMin, ArmLengthMax);
    }

    void ClampPitch() { BoomPitchDegrees = std::clamp(BoomPitchDegrees, PitchMin, PitchMax); }

    [[nodiscard]] glm::vec3 GetTargetLocation(const glm::vec3& ActorLocation) const;

    /// Snap lagged state to desired (call on possess / level enter).
    void SnapLagState(const glm::vec3& ActorLocation);

    /// Movement uses *desired* boom yaw so controls stay responsive while the view lags.
    [[nodiscard]] glm::vec3 GetMoveDirectionXZ(const FMoveAxes2D& Axes) const {
        return YawRelativeMoveXz(BoomYawDegrees, Axes);
    }

    /// World yaw for a pawn facing the same XZ direction the orbit camera looks
    /// (matches `yawRelativeMoveXZ` forward / crosshair aim on the ground plane).
    [[nodiscard]] float GetLookFacingYawDegrees() const;

    /// Advance lag, optional collision probe, and push the Engine orbit camera.
    /// If `physScene` is null, uses `GetOwner()->GetWorld()->GetPhysicsScene()` when available.
    void ApplyToCamera(UCameraComponent& Camera, const glm::vec3& ActorLocation, float DeltaTime,
                       FPhysScene* PhysScene = nullptr, FDebugDraw* DebugDraw = nullptr);

    /// Prefer when attached under an Actor root: uses owner location + world FPhysScene.
    void ApplyToCamera(UCameraComponent& Camera, float DeltaTime, FDebugDraw* DebugDraw = nullptr);

    /// Unit boom direction matching `Camera` orbit eye offset (target → camera).
    [[nodiscard]] static glm::vec3 GetBoomDirection(float YawDegrees, float PitchDegrees);

    /// Sphere-sweep arm length; returns clamped length (ArmLengthMin..desiredLength).
    [[nodiscard]] float ProbeArmLength(FPhysScene& PhysScene, const glm::vec3& Target,
                                       float YawDegrees, float PitchDegrees, float DesiredLength,
                                       FDebugDraw* DebugDraw = nullptr) const;

private:
    [[nodiscard]] static float ExpSmoothAlpha(float Speed, float DeltaTime);
    [[nodiscard]] static float LerpAngleDegrees(float FromDegrees, float ToDegrees, float Alpha);
    void UpdateLag(float DeltaTime, const glm::vec3& ActorLocation);

    glm::vec3 LaggedTarget{0.0f};
    float LaggedYawDegrees = 0.0f;
    float LaggedPitchDegrees = 15.0f;
    float LaggedArmLength = 4.0f;
    bool bLagInitialized = false;
};


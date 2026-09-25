#pragma once

#include "Camera/CameraComponent.h"
#include "CollisionQuery.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Input.h"

class FDebugDraw;
class FPhysScene;

/**
 * Unreal-like Spring Arm / Camera Boom (USceneComponent) with optional camera lag,
 * rotation lag, smoothed arm length, and collision probe (sphere sweep).
 */
class ENGINE_API USpringArmComponent : public USceneComponent
{
public:
	/** Desired boom length in cm (scroll edits this; lag follows toward it). */
	float TargetArmLength = 400.0f;
	/** cm */
	float ArmLengthMin = 150.0f;
	/** cm */
	float ArmLengthMax = 2000.0f;
	/** Height of the boom target above the actor location (cm). */
	float SocketOffsetZ = 100.0f;
	/** Unreal-like SocketOffset.X (cm) — positive = right of pawn along boom right (over-shoulder). */
	float SocketOffsetX = 0.0f;

	/** Desired boom orientation (mouse look edits these immediately). */
	float BoomYawDegrees = 0.0f;
	float BoomPitchDegrees = 15.0f;
	float PitchMin = -60.0f;
	float PitchMax = 70.0f;

	/** Unreal-style follow lag (higher speed = snappier). */
	bool bEnableCameraLag = true;
	float CameraLagSpeed = 10.0f;
	bool bEnableCameraRotationLag = true;
	float CameraRotationLagSpeed = 14.0f;
	/** Smooth zoom toward TargetArmLength. */
	float ArmLengthLagSpeed = 10.0f;

	/** Unreal bDoCollisionTest — sphere-sweep target → camera against FPhysScene. */
	bool bDoCollisionTest = true;
	/** Sphere probe radius (cm). Unreal ProbeSize defaults to 12. */
	float ProbeSize = 15.0f;
	/** Extra pull-in after a hit so the near plane stays clear of geometry (cm). */
	float CollisionProbeOffset = 5.0f;
	ECollisionChannel ProbeChannel = ECollisionChannel::WorldStatic;

	void AddYawInput(float DeltaDegrees)
	{
		BoomYawDegrees += DeltaDegrees;
	}

	void AddPitchInput(float DeltaDegrees)
	{
		BoomPitchDegrees = FMath::Clamp(BoomPitchDegrees + DeltaDegrees, PitchMin, PitchMax);
	}

	/** Positive delta lengthens the boom (zoom out). Clamped to ArmLengthMin/Max. */
	void AddArmLengthInput(float DeltaLength)
	{
		TargetArmLength = FMath::Clamp(TargetArmLength + DeltaLength, ArmLengthMin, ArmLengthMax);
	}

	void ClampPitch()
	{
		BoomPitchDegrees = FMath::Clamp(BoomPitchDegrees, PitchMin, PitchMax);
	}

	[[nodiscard]] FVector GetTargetLocation(const FVector& ActorLocation) const;

	/** Snap lagged state to desired (call on possess / level enter). */
	void SnapLagState(const FVector& ActorLocation);

	/** Movement uses *desired* boom yaw so controls stay responsive while the view lags. */
	[[nodiscard]] FVector GetMoveDirectionXZ(const FMoveAxes2D& Axes) const
	{
		return YawRelativeMoveXz(BoomYawDegrees, Axes);
	}

	/**
	 * World yaw for a pawn facing the same XZ direction the orbit camera looks
	 * (matches yawRelativeMoveXZ forward / crosshair aim on the ground plane).
	 */
	[[nodiscard]] float GetLookFacingYawDegrees() const;

	/**
	 * Advance lag, optional collision probe, and push the Engine orbit camera.
	 * If physScene is null, uses GetOwner()->GetWorld()->GetPhysicsScene() when available.
	 */
	void ApplyToCamera(UCameraComponent& Camera, const FVector& ActorLocation, float DeltaTime,
		FPhysScene* PhysScene = nullptr, FDebugDraw* DebugDraw = nullptr);

	/** Prefer when attached under an Actor root: uses owner location + world FPhysScene. */
	void ApplyToCamera(UCameraComponent& Camera, float DeltaTime, FDebugDraw* DebugDraw = nullptr);

	/** Unit boom direction matching Camera orbit eye offset (target → camera). */
	[[nodiscard]] static FVector GetBoomDirection(float YawDegrees, float PitchDegrees);

	/** Sphere-sweep arm length; returns clamped length (ArmLengthMin..desiredLength). */
	[[nodiscard]] float ProbeArmLength(FPhysScene& PhysScene, const FVector& Target, float YawDegrees,
		float PitchDegrees, float DesiredLength, FDebugDraw* DebugDraw = nullptr) const;

private:
	[[nodiscard]] static float ExpSmoothAlpha(float Speed, float DeltaTime);
	[[nodiscard]] static float LerpAngleDegrees(float FromDegrees, float ToDegrees, float Alpha);
	void UpdateLag(float DeltaTime, const FVector& ActorLocation);

	FVector LaggedTarget = FVector::ZeroVector;
	float LaggedYawDegrees = 0.0f;
	float LaggedPitchDegrees = 15.0f;
	float LaggedArmLength = 400.0f;
	bool bLagInitialized = false;
};

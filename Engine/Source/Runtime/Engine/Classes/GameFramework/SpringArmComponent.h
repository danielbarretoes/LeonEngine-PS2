#pragma once

#include "Camera/CameraComponent.h"
#include "CollisionQuery.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SpringArmComponent.generated.h"

class FDebugDraw;
class FPhysScene;

/**
 * Unreal-like Spring Arm / Camera Boom (USceneComponent) with optional camera lag,
 * rotation lag, smoothed arm length, and collision probe (sphere sweep).
 *
 * The arm's rotation is the view rotation (UE: GetTargetRotation): the camera sits ArmLength behind the arm origin,
 * at Origin - Rotation.Vector() * ArmLength, and looks along the rotation. With bUsePawnControlRotation the rotation is
 * the owning pawn's view (control) rotation, otherwise the component's own rotation.
 */
UCLASS()
class ENGINE_API USpringArmComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	USpringArmComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Desired boom length in cm (scroll edits this; lag follows toward it). */
	UPROPERTY()
	float TargetArmLength = 400.0f;

	/** cm */
	UPROPERTY()
	float ArmLengthMin = 150.0f;

	/** cm */
	UPROPERTY()
	float ArmLengthMax = 2000.0f;

	/** Offset of the arm origin from the owner's location, in world space (cm; UE: TargetOffset). */
	UPROPERTY()
	FVector TargetOffset = FVector(0.0f, 0.0f, 100.0f);

	/**
	 * Offset in the arm's rotation space (cm; UE: SocketOffset): Y > 0 puts the camera right of the pawn (over the
	 * shoulder). Unlike UE, which moves only the end of the arm, it moves the whole arm: the camera orbits the offset
	 * point and the collision probe starts there.
	 */
	UPROPERTY()
	FVector SocketOffset = FVector::ZeroVector;

	/** Use the owning pawn's view rotation (UE: bUsePawnControlRotation); otherwise the component's rotation. */
	UPROPERTY()
	bool bUsePawnControlRotation = false;

	/** Unreal-style follow lag (higher speed = snappier). */
	UPROPERTY()
	bool bEnableCameraLag = true;

	UPROPERTY()
	float CameraLagSpeed = 10.0f;

	UPROPERTY()
	bool bEnableCameraRotationLag = true;

	UPROPERTY()
	float CameraRotationLagSpeed = 14.0f;

	/** Smooth zoom toward TargetArmLength. */
	UPROPERTY()
	float ArmLengthLagSpeed = 10.0f;

	/** Unreal bDoCollisionTest — sphere-sweep target → camera against FPhysScene. */
	UPROPERTY()
	bool bDoCollisionTest = true;

	/** Sphere probe radius (cm). Unreal ProbeSize defaults to 12. */
	UPROPERTY()
	float ProbeSize = 15.0f;

	/** Extra pull-in after a hit so the near plane stays clear of geometry (cm). */
	UPROPERTY()
	float CollisionProbeOffset = 5.0f;

	/** The channel of the collision probe (ECollisionChannel is not reflected yet). */
	ECollisionChannel ProbeChannel = ECollisionChannel::WorldStatic;

	/** Positive delta lengthens the boom (zoom out). Clamped to ArmLengthMin/Max. */
	void AddArmLengthInput(float DeltaLength)
	{
		TargetArmLength = FMath::Clamp(TargetArmLength + DeltaLength, ArmLengthMin, ArmLengthMax);
	}

	/** The desired arm rotation (UE: GetTargetRotation): the pawn's view rotation or the component's rotation. */
	[[nodiscard]] FRotator GetTargetRotation() const;

	/** The arm origin, where the camera looks, for an owner location: TargetOffset, then the rotated SocketOffset. */
	[[nodiscard]] FVector GetArmOrigin(const FVector& ActorLocation) const;

	/** Snap lagged state to desired (call on possess / level enter). */
	void SnapLagState(const FVector& ActorLocation);

	/**
	 * Advance lag, optional collision probe, and push the Engine orbit camera.
	 * If physScene is null, uses GetOwner()->GetWorld()->GetPhysicsScene() when available.
	 */
	void ApplyToCamera(UCameraComponent& Camera, const FVector& ActorLocation, float DeltaTime,
		FPhysScene* PhysScene = nullptr, FDebugDraw* DebugDraw = nullptr);

	/** Prefer when attached under an Actor root: uses owner location + world FPhysScene. */
	void ApplyToCamera(UCameraComponent& Camera, float DeltaTime, FDebugDraw* DebugDraw = nullptr);

	/**
	 * Sphere-sweep the arm from Origin back along Rotation (toward the camera); returns the clamped length
	 * (ArmLengthMin..DesiredLength).
	 */
	[[nodiscard]] float ProbeArmLength(FPhysScene& PhysScene, const FVector& Origin, const FRotator& Rotation,
		float DesiredLength, FDebugDraw* DebugDraw = nullptr) const;

private:
	[[nodiscard]] static float ExpSmoothAlpha(float Speed, float DeltaTime);
	[[nodiscard]] static float LerpAngleDegrees(float FromDegrees, float ToDegrees, float Alpha);
	void UpdateLag(float DeltaTime, const FVector& ActorLocation);

	FVector LaggedOrigin = FVector::ZeroVector;
	FRotator LaggedRotation = FRotator::ZeroRotator;
	float LaggedArmLength = 400.0f;
	bool bLagInitialized = false;
};

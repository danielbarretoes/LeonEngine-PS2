#pragma once

#include "Camera/CameraTypes.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "CameraComponent.generated.h"

UENUM()
enum class ECameraMode : uint8
{
	Orbit, // Blender-style tumble around a target: the eye is Target - ViewRotation.Vector() * Distance
	FreeLook, // Unreal-like flying / first-person: eye + view rotation
};

/** Default perspective clip planes of the engine camera (world units, cm). */
inline constexpr float DefaultCameraNearPlane = 10.0f;
inline constexpr float DefaultCameraFarPlane = 10000.0f;
/** The near plane of the view model pass: a first-person weapon sits a few centimetres from the eye (cm). */
inline constexpr float ViewModelNearPlane = 1.0f;

/**
 * View camera (UE: UCameraComponent, a scene component): orbit (default) or free-look. Both modes look along a UE view
 * rotation (yaw about Z from +X toward +Y, pitch up from the horizontal; roll is always 0).
 *
 * Unlike UE's camera, the view does not come from the component transform by default: the eye comes from the orbit
 * target / distance or the free-look eye, and a spring arm pushes into it with ApplyToCamera. With
 * bUsePawnControlRotation (UE's first-person camera, P17) it does: the eye is the component's world location (attached
 * to the pawn, e.g. at the eyes) and the view rotation is the owning pawn's control rotation. The player camera
 * manager keeps the player's view in one (APlayerCameraManager::GetViewCamera), in free look, and the renderer draws
 * with it.
 */
UCLASS()
class ENGINE_API UCameraComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UCameraComponent(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void SetPerspective(float InFovDegrees, float InAspect, float InNearPlane, float InFarPlane);
	/** Orthographic projection; height is the full vertical world extent visible. */
	void SetOrthographic(float Height, float InAspect, float InNearPlane, float InFarPlane);

	/** Vertical FOV in degrees (rebuilds projection with last aspect/near/far). */
	void SetFieldOfView(float InFovDegrees);
	[[nodiscard]] float FieldOfView() const
	{
		return FovDegrees;
	}
	[[nodiscard]] float GetAspect() const
	{
		return Aspect;
	}
	[[nodiscard]] float GetNearPlane() const
	{
		return NearPlane;
	}
	[[nodiscard]] float GetFarPlane() const
	{
		return FarPlane;
	}
	[[nodiscard]] bool IsOrthographic() const
	{
		return bOrthographic;
	}
	[[nodiscard]] float GetOrthoHeight() const
	{
		return OrthoHeight;
	}
	void SetOrthoHeight(float Height);

	void SetMode(ECameraMode InMode);
	[[nodiscard]] ECameraMode GetMode() const
	{
		return Mode;
	}

	/**
	 * Adds to the view rotation (UE: AddWorldRotation of the yaw and pitch): a positive yaw turns the view right, a
	 * positive pitch looks up (clamped to +-89); the roll is ignored. Orbit: the eye tumbles around the target.
	 * FreeLook: the look direction turns.
	 */
	void AddViewRotation(const FRotator& DeltaRotation);

	/** Slide along camera right / world up (Orbit moves pivot; FreeLook moves eye). */
	void Pan(float DeltaRight, float DeltaUp);

	void Zoom(float DeltaDistance);
	void SetDistance(float InDistance);
	/** Sets the view rotation (the roll is dropped; the pitch is clamped to +-89). */
	void SetViewRotation(const FRotator& InRotation);
	[[nodiscard]] const FRotator& GetViewRotation() const
	{
		return ViewRotation;
	}

	/** World to UE view space: x = right, y = up, z = forward (ViewMatrices.h). */
	[[nodiscard]] FMatrix ViewMatrix() const;
	/**
	 * View to UE clip space: depth z / w in [0, 1], 0 at the near plane (FPerspectiveMatrix / FOrthoMatrix). The GL
	 * renderer converts it with ToGLClipSpace (GLClipSpace.h).
	 */
	[[nodiscard]] const FMatrix& ProjectionMatrix() const
	{
		return Projection;
	}
	/** Eye position (orbit: derived from target+distance; FreeLook: explicit eye). */
	[[nodiscard]] FVector GetCameraLocation() const;

	[[nodiscard]] float GetDistance() const
	{
		return Distance;
	}
	[[nodiscard]] const FVector& GetTarget() const
	{
		return Target;
	}
	void SetTarget(const FVector& InTarget);

	/** FreeLook eye (ignored in Orbit mode). */
	void SetEyeLocation(const FVector& InEye);
	[[nodiscard]] const FVector& EyeLocation() const
	{
		return Eye;
	}

	/** Unit look / strafe vectors of the view rotation (the strafe vector stays horizontal). */
	[[nodiscard]] FVector ForwardVector() const;
	[[nodiscard]] FVector RightVector() const;

	/**
	 * The camera's view for a player camera manager (UE: GetCameraView, from AActor::CalcCamera). With
	 * bUsePawnControlRotation the view looks from the component's world location along the owning pawn's view
	 * rotation (both kept as the free-look eye and rotation); otherwise it is the orbit / free-look view.
	 */
	virtual void GetCameraView(float DeltaTime, FMinimalViewInfo& DesiredView);

	/**
	 * The view follows the owning pawn's control rotation and the eye is the component's location (UE:
	 * bUsePawnControlRotation, the first-person camera): GetCameraView also turns the component to the view rotation,
	 * so what is attached to it (a first-person weapon) follows the aim. Off by default.
	 */
	UPROPERTY()
	bool bUsePawnControlRotation = false;

	/**
	 * The vertical field of view of the view model primitives this camera shows (UPrimitiveComponent::
	 * bRenderAsViewModel), degrees; 0 uses the camera's. Leon: UE 4.27 has no view model pass (games scale the weapon
	 * in a material or capture it), so the renderer draws them last, after a depth clear, with this projection.
	 */
	UPROPERTY()
	float ViewModelFOV = 0.0f;

private:
	void InvalidateCache();
	void UpdateCachedPosition() const;

	UPROPERTY()
	ECameraMode Mode = ECameraMode::Orbit;

	/** Rebuilt by SetPerspective / SetOrthographic (FMatrix is not reflected). */
	FMatrix Projection = FMatrix::Identity;

	/** Orbit pivot (cm). */
	UPROPERTY()
	FVector Target = FVector::ZeroVector;

	/** Free-look eye. World units (cm), like every length of the camera. */
	UPROPERTY()
	FVector Eye = FVector(0.0f, 0.0f, 100.0f);

	/** Orbit default: the eye 25 degrees above the target, looking down at it. */
	UPROPERTY()
	FRotator ViewRotation = FRotator(-25.0f, 225.0f, 0.0f);

	/** Orbit distance (cm). */
	UPROPERTY()
	float Distance = 500.0f;

	/** Vertical field of view in degrees (UE: FieldOfView, horizontal there). */
	UPROPERTY()
	float FovDegrees = 60.0f;

	/** UE: AspectRatio. */
	UPROPERTY()
	float Aspect = 16.0f / 9.0f;

	UPROPERTY()
	float NearPlane = DefaultCameraNearPlane;

	UPROPERTY()
	float FarPlane = DefaultCameraFarPlane;

	/** UE: ProjectionMode == Orthographic. */
	UPROPERTY()
	bool bOrthographic = false;

	/** Full vertical extent of the orthographic view (cm; UE: OrthoWidth, horizontal there). */
	UPROPERTY()
	float OrthoHeight = 2000.0f;

	mutable bool bCacheDirty = true;
	mutable FVector CachedPosition = FVector::ZeroVector;
};

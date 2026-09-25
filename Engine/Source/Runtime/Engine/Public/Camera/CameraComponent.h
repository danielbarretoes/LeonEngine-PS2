#pragma once

#include "CoreMinimal.h"

enum class ECameraMode : uint8
{
	Orbit, // Blender-style tumble around a target
	FreeLook, // Unreal-like flying / first-person: eye + look yaw/pitch
};

/** Default perspective clip planes of the engine camera (world units, cm). */
inline constexpr float DefaultCameraNearPlane = 10.0f;
inline constexpr float DefaultCameraFarPlane = 10000.0f;

/** View camera: orbit (default) or free-look for ADefaultCameraActor. */
class ENGINE_API UCameraComponent
{
public:
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

	/** Orbit: tumble around target. FreeLook: add yaw/pitch to look direction. */
	void Orbit(float DeltaYawDegrees, float DeltaPitchDegrees);
	void AddLook(float DeltaYawDegrees, float DeltaPitchDegrees)
	{
		Orbit(DeltaYawDegrees, DeltaPitchDegrees);
	}

	/** Slide along camera right / world up (Orbit moves pivot; FreeLook moves eye). */
	void Pan(float DeltaRight, float DeltaUp);

	void Zoom(float DeltaDistance);
	void SetDistance(float InDistance);
	void SetYawPitch(float InYawDegrees, float InPitchDegrees);

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
	[[nodiscard]] float GetYawDegrees() const
	{
		return YawDegrees;
	}
	[[nodiscard]] float GetPitchDegrees() const
	{
		return PitchDegrees;
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

	/** Unit look / strafe vectors for the active mode. */
	[[nodiscard]] FVector ForwardVector() const;
	[[nodiscard]] FVector RightVector() const;

private:
	void InvalidateCache();
	void UpdateCachedPosition() const;

	ECameraMode Mode = ECameraMode::Orbit;
	FMatrix Projection = FMatrix::Identity;
	FVector Target = FVector::ZeroVector;
	/** World units (cm), like every length of the camera. */
	FVector Eye = FVector(0.0f, 100.0f, 0.0f);
	float YawDegrees = 45.0f;
	float PitchDegrees = 25.0f;
	float Distance = 500.0f;
	float FovDegrees = 60.0f;
	float Aspect = 16.0f / 9.0f;
	float NearPlane = DefaultCameraNearPlane;
	float FarPlane = DefaultCameraFarPlane;
	bool bOrthographic = false;
	float OrthoHeight = 2000.0f;

	mutable bool bCacheDirty = true;
	mutable FVector CachedPosition = FVector::ZeroVector;
};

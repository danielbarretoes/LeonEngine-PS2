#include "Camera/CameraComponent.h"

#include "ViewMatrices.h"

namespace
{

	[[nodiscard]] FVector FreeLookForward(float InYawDegrees, float InPitchDegrees)
	{
		const float YawRad = FMath::DegreesToRadians(InYawDegrees);
		const float PitchRad = FMath::DegreesToRadians(InPitchDegrees);
		return FVector(
			FMath::Cos(PitchRad) * FMath::Cos(YawRad), FMath::Sin(PitchRad), FMath::Cos(PitchRad) * FMath::Sin(YawRad))
			.GetUnsafeNormal();
	}

	/** Stable up for lookAt when looking nearly straight up/down (ortho Top). */
	[[nodiscard]] FVector FreeLookWorldUp(float InYawDegrees, float InPitchDegrees)
	{
		if (InPitchDegrees < -80.0f)
		{
			const float YawRad = FMath::DegreesToRadians(InYawDegrees);
			return FVector(FMath::Cos(YawRad), 0.0f, FMath::Sin(YawRad)).GetUnsafeNormal();
		}
		if (InPitchDegrees > 80.0f)
		{
			const float YawRad = FMath::DegreesToRadians(InYawDegrees);
			return FVector(-FMath::Cos(YawRad), 0.0f, -FMath::Sin(YawRad)).GetUnsafeNormal();
		}
		return FVector(0.0f, 1.0f, 0.0f);
	}

} // namespace

void UCameraComponent::SetPerspective(float InFovDegrees, float InAspect, float InNearPlane, float InFarPlane)
{
	FovDegrees = FMath::Clamp(InFovDegrees, 20.0f, 120.0f);
	Aspect = InAspect > 1.0e-4f ? InAspect : (16.0f / 9.0f);
	NearPlane = InNearPlane;
	FarPlane = InFarPlane;
	bOrthographic = false;
	// UE clip space, depth 0 at the near plane and 1 at the far one. The vertical field of view is kept: the half
	// angle is the same on both axes and x is scaled down by the aspect ratio.
	const float HalfFov = FMath::DegreesToRadians(FovDegrees) / 2.0f;
	Projection = FPerspectiveMatrix(HalfFov, HalfFov, 1.0f / Aspect, 1.0f, NearPlane, FarPlane);
}

void UCameraComponent::SetOrthographic(float Height, float InAspect, float InNearPlane, float InFarPlane)
{
	OrthoHeight = FMath::Clamp(Height, 0.5f, 500.0f);
	Aspect = InAspect > 1.0e-4f ? InAspect : (16.0f / 9.0f);
	NearPlane = InNearPlane;
	FarPlane = InFarPlane;
	bOrthographic = true;
	const float HalfH = OrthoHeight * 0.5f;
	const float HalfW = HalfH * Aspect;
	// UE clip space: depth (z - Near) / (Far - Near).
	Projection = FOrthoMatrix(HalfW, HalfH, 1.0f / (FarPlane - NearPlane), -NearPlane);
}

void UCameraComponent::SetOrthoHeight(float Height)
{
	if (bOrthographic)
	{
		SetOrthographic(Height, Aspect, NearPlane, FarPlane);
	}
	else
	{
		OrthoHeight = FMath::Clamp(Height, 0.5f, 500.0f);
	}
}

void UCameraComponent::SetFieldOfView(float InFovDegrees)
{
	SetPerspective(InFovDegrees, Aspect, NearPlane, FarPlane);
}

void UCameraComponent::SetMode(ECameraMode InMode)
{
	if (Mode == InMode)
	{
		return;
	}
	Mode = InMode;
	InvalidateCache();
}

void UCameraComponent::Orbit(float DeltaYawDegrees, float DeltaPitchDegrees)
{
	YawDegrees += DeltaYawDegrees;
	PitchDegrees = FMath::Clamp(PitchDegrees + DeltaPitchDegrees, -89.0f, 89.0f);
	InvalidateCache();
}

void UCameraComponent::Pan(float DeltaRight, float DeltaUp)
{
	const FVector Right = RightVector();
	const FVector Up = FVector(0.0f, 1.0f, 0.0f);
	const FVector Delta = Right * DeltaRight + Up * DeltaUp;
	if (Mode == ECameraMode::FreeLook)
	{
		Eye += Delta;
	}
	else
	{
		Target += Delta;
	}
	InvalidateCache();
}

void UCameraComponent::Zoom(float DeltaDistance)
{
	if (Mode != ECameraMode::Orbit)
	{
		return;
	}
	SetDistance(Distance - DeltaDistance);
}

void UCameraComponent::SetDistance(float InDistance)
{
	Distance = FMath::Clamp(InDistance, 0.5f, 80.0f);
	InvalidateCache();
}

void UCameraComponent::SetYawPitch(float InYawDegrees, float InPitchDegrees)
{
	YawDegrees = InYawDegrees;
	PitchDegrees = FMath::Clamp(InPitchDegrees, -89.0f, 89.0f);
	InvalidateCache();
}

void UCameraComponent::SetTarget(const FVector& InTarget)
{
	Target = InTarget;
	InvalidateCache();
}

void UCameraComponent::SetEyeLocation(const FVector& InEye)
{
	Eye = InEye;
	InvalidateCache();
}

void UCameraComponent::InvalidateCache()
{
	bCacheDirty = true;
}

void UCameraComponent::UpdateCachedPosition() const
{
	if (!bCacheDirty)
	{
		return;
	}

	if (Mode == ECameraMode::FreeLook)
	{
		CachedPosition = Eye;
		bCacheDirty = false;
		return;
	}

	const float YawRad = FMath::DegreesToRadians(YawDegrees);
	const float PitchRad = FMath::DegreesToRadians(PitchDegrees);

	CachedPosition = Target +
		FVector(Distance * FMath::Cos(PitchRad) * FMath::Cos(YawRad), Distance * FMath::Sin(PitchRad),
			Distance * FMath::Cos(PitchRad) * FMath::Sin(YawRad));
	bCacheDirty = false;
}

FVector UCameraComponent::GetCameraLocation() const
{
	UpdateCachedPosition();
	return CachedPosition;
}

FVector UCameraComponent::ForwardVector() const
{
	if (Mode == ECameraMode::FreeLook)
	{
		return FreeLookForward(YawDegrees, PitchDegrees);
	}
	UpdateCachedPosition();
	const FVector ToTarget = Target - CachedPosition;
	const float Len = ToTarget.Size();
	if (Len < 1.0e-5f)
	{
		return FVector(0.0f, 0.0f, -1.0f);
	}
	return ToTarget / Len;
}

FVector UCameraComponent::RightVector() const
{
	const FVector Forward = ForwardVector();
	const FVector Up =
		(Mode == ECameraMode::FreeLook) ? FreeLookWorldUp(YawDegrees, PitchDegrees) : FVector(0.0f, 1.0f, 0.0f);
	FVector Right = FVector::CrossProduct(Forward, Up);
	const float Len = Right.Size();
	if (Len < 1.0e-5f)
	{
		Right = FVector::CrossProduct(Forward, FVector(0.0f, 0.0f, 1.0f));
		const float Len2 = Right.Size();
		if (Len2 < 1.0e-5f)
		{
			return FVector(1.0f, 0.0f, 0.0f);
		}
		return Right / Len2;
	}
	return Right / Len;
}

FMatrix UCameraComponent::ViewMatrix() const
{
	UpdateCachedPosition();
	if (Mode == ECameraMode::FreeLook)
	{
		const FVector Forward = FreeLookForward(YawDegrees, PitchDegrees);
		const FVector Up = FreeLookWorldUp(YawDegrees, PitchDegrees);
		return MakeLookAtView(CachedPosition, CachedPosition + Forward, Up);
	}
	return MakeLookAtView(CachedPosition, Target, FVector(0.0f, 1.0f, 0.0f));
}

#include "Camera/CameraComponent.h"

#include "GameFramework/Pawn.h"
#include "ViewMatrices.h"

namespace
{

	/** Orbit distance range (cm). */
	constexpr float MinOrbitDistance = 50.0f;
	constexpr float MaxOrbitDistance = 8000.0f;
	/** Orthographic view height range (cm). */
	constexpr float MinOrthoHeight = 50.0f;
	constexpr float MaxOrthoHeight = 50000.0f;

	/** Largest view pitch up or down (degrees). */
	constexpr float MaxViewPitch = 89.0f;

} // namespace

UCameraComponent::UCameraComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UCameraComponent::SetPerspective(float InFovDegrees, float InAspect, float InNearPlane, float InFarPlane)
{
	// From a sniper scope's zoom to a wide view.
	FovDegrees = FMath::Clamp(InFovDegrees, 5.0f, 120.0f);
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
	OrthoHeight = FMath::Clamp(Height, MinOrthoHeight, MaxOrthoHeight);
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
		OrthoHeight = FMath::Clamp(Height, MinOrthoHeight, MaxOrthoHeight);
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

void UCameraComponent::AddViewRotation(const FRotator& DeltaRotation)
{
	ViewRotation.Yaw += DeltaRotation.Yaw;
	ViewRotation.Pitch = FMath::Clamp(ViewRotation.Pitch + DeltaRotation.Pitch, -MaxViewPitch, MaxViewPitch);
	InvalidateCache();
}

void UCameraComponent::Pan(float DeltaRight, float DeltaUp)
{
	const FVector Right = RightVector();
	const FVector Up = FVector(0.0f, 0.0f, 1.0f);
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
	Distance = FMath::Clamp(InDistance, MinOrbitDistance, MaxOrbitDistance);
	InvalidateCache();
}

void UCameraComponent::GetCameraView(float /*DeltaTime*/, FMinimalViewInfo& DesiredView)
{
	if (bUsePawnControlRotation)
	{
		// UE: the pawn's view rotation (its controller's control rotation) and the component's location.
		if (const APawn* OwningPawn = Cast<APawn>(GetOwner()))
		{
			SetMode(ECameraMode::FreeLook);
			const FRotator PawnViewRotation = OwningPawn->GetViewRotation();
			SetViewRotation(PawnViewRotation);
			SetEyeLocation(GetComponentLocation());
			// UE: the component turns with the view, and what is attached to it (a first-person weapon) with it.
			if (!PawnViewRotation.Equals(GetComponentRotation()))
			{
				SetWorldRotation(PawnViewRotation);
			}
		}
	}
	DesiredView.Location = GetCameraLocation();
	DesiredView.Rotation = GetViewRotation();
	DesiredView.FOV = FieldOfView();
	DesiredView.ViewModelFOV = ViewModelFOV;
}

void UCameraComponent::SetViewRotation(const FRotator& InRotation)
{
	ViewRotation = FRotator(FMath::Clamp(InRotation.Pitch, -MaxViewPitch, MaxViewPitch), InRotation.Yaw, 0.0f);
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

	// The eye sits behind the target along the view direction.
	CachedPosition = Target - (ViewRotation.Vector() * Distance);
	bCacheDirty = false;
}

FVector UCameraComponent::GetCameraLocation() const
{
	UpdateCachedPosition();
	return CachedPosition;
}

FVector UCameraComponent::ForwardVector() const
{
	return ViewRotation.Vector();
}

FVector UCameraComponent::RightVector() const
{
	// The view's right axis; with no roll it is horizontal: WorldUp ^ Forward in the left-handed world.
	return FRotationMatrix(ViewRotation).GetUnitAxis(EAxis::Y);
}

FMatrix UCameraComponent::ViewMatrix() const
{
	UpdateCachedPosition();
	return MakeViewMatrix(CachedPosition, ViewRotation);
}

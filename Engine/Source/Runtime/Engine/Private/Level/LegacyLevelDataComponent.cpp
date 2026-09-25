#include "Level/LegacyLevelDataComponent.h"

#include "UObject/Package.h"

ULegacyLevelDataComponent::ULegacyLevelDataComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void ULegacyLevelDataComponent::ApplyCameraFraming(UCameraComponent& Camera) const
{
	Camera.SetTarget(CameraTarget);
	Camera.SetDistance(CameraDistance);
	Camera.SetViewRotation(CameraViewRotation);
	Camera.SetEyeLocation(CameraEye);
	Camera.SetMode(CameraMode);
}

void ULegacyLevelDataComponent::GetPlayFromHereView(FVector& OutLocation, FRotator& OutRotation) const
{
	// The legacy engine camera took the framing, then its default game mode flew from the eye looking at the target
	// (an orbit framing keeps its view, a free-look one turns to the target): the same camera math gives the same view.
	UCameraComponent* Camera = NewObject<UCameraComponent>(GetTransientPackage());
	ApplyCameraFraming(*Camera);
	const FVector Eye = Camera->GetCameraLocation();
	FVector Look = Camera->GetTarget() - Eye;
	const float LookLen = Look.Size();
	if (LookLen > 1.0e-3f)
	{
		Look /= LookLen;
	}
	else
	{
		Look = FVector(0.0f, -1.0f, 0.0f);
	}
	Camera->SetMode(ECameraMode::FreeLook);
	Camera->SetEyeLocation(Eye);
	Camera->SetViewRotation(Look.Rotation());
	OutLocation = Camera->EyeLocation();
	OutRotation = Camera->GetViewRotation();
	Camera->MarkPendingKill();
}

#include "Camera/PlayerCameraManager.h"

#include "GameFramework/PlayerController.h"

APlayerCameraManager::APlayerCameraManager(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bHidden = true;
	ViewCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("ViewCamera"));
}

void APlayerCameraManager::InitializeFor(APlayerController* PC)
{
	PCOwner = PC;
	CameraCachePOV.FOV = DefaultFOV;
}

void APlayerCameraManager::SetViewTarget(AActor* NewViewTarget)
{
	ViewTargetActor = NewViewTarget;
}

AActor* APlayerCameraManager::GetViewTarget() const
{
	if (ViewTargetActor != nullptr && !ViewTargetActor->IsPendingKillPending())
	{
		return ViewTargetActor;
	}
	if (PCOwner != nullptr && PCOwner->GetPawn() != nullptr)
	{
		return PCOwner->GetPawn();
	}
	return PCOwner;
}

void APlayerCameraManager::UpdateCamera(float DeltaTime)
{
	FMinimalViewInfo POV;
	POV.FOV = DefaultFOV;
	AActor* Target = GetViewTarget();
	if (Target != nullptr)
	{
		Target->CalcCamera(DeltaTime, POV);
	}
	if (LockedFOV > 0.0f)
	{
		POV.FOV = LockedFOV;
	}
	// The last view is the one to draw from; a first view, a new target or a jump has nothing to come from.
	const bool bContinuous = bHasCameraCache && LastViewTarget.Get() == Target &&
		(POV.Location - CameraCachePOV.Location).SizeSquared() <= FMath::Square(TeleportDistance);
	LastFrameCameraCachePOV = bContinuous ? CameraCachePOV : POV;
	LastViewTarget = Target;
	CameraCachePOV = POV;
	bHasCameraCache = true;

	// The view camera looks from the point of view (Leon: its free-look mode builds the renderer's matrices).
	ViewCamera->SetMode(ECameraMode::FreeLook);
	ViewCamera->SetViewRotation(POV.Rotation);
	ViewCamera->SetEyeLocation(POV.Location);
	if (ViewCamera->FieldOfView() != POV.FOV)
	{
		ViewCamera->SetFieldOfView(POV.FOV);
	}
	ViewCamera->ViewModelFOV = POV.ViewModelFOV;
}

void APlayerCameraManager::GetInterpolatedView(float Alpha, FMinimalViewInfo& OutView) const
{
	const float Weight = FMath::Clamp(Alpha, 0.0f, 1.0f);
	OutView = CameraCachePOV;
	OutView.Location = FMath::Lerp(LastFrameCameraCachePOV.Location, CameraCachePOV.Location, Weight);
	// The shortest way around for the yaw and the roll (a turn through 180 degrees must not spin the other way).
	const FRotator Delta = (CameraCachePOV.Rotation - LastFrameCameraCachePOV.Rotation).GetNormalized();
	OutView.Rotation = LastFrameCameraCachePOV.Rotation + (Delta * Weight);
	OutView.FOV = FMath::Lerp(LastFrameCameraCachePOV.FOV, CameraCachePOV.FOV, Weight);
}

float APlayerCameraManager::GetFOVAngle() const
{
	return LockedFOV > 0.0f ? LockedFOV : CameraCachePOV.FOV;
}

void APlayerCameraManager::SetFOV(float NewFOV)
{
	LockedFOV = NewFOV;
}

void APlayerCameraManager::UnlockFOV()
{
	LockedFOV = 0.0f;
}

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
	if (AActor* Target = GetViewTarget())
	{
		Target->CalcCamera(DeltaTime, POV);
	}
	if (LockedFOV > 0.0f)
	{
		POV.FOV = LockedFOV;
	}
	CameraCachePOV = POV;

	// The view camera looks from the point of view (Leon: its free-look mode builds the renderer's matrices).
	ViewCamera->SetMode(ECameraMode::FreeLook);
	ViewCamera->SetViewRotation(POV.Rotation);
	ViewCamera->SetEyeLocation(POV.Location);
	if (ViewCamera->FieldOfView() != POV.FOV)
	{
		ViewCamera->SetFieldOfView(POV.FOV);
	}
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

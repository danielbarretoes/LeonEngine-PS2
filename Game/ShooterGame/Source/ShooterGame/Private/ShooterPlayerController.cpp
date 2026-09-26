#include "ShooterPlayerController.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "ShooterGame.h"

AShooterPlayerController::AShooterPlayerController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AShooterPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindAction(TEXT("Scoreboard"), IE_Pressed, this, &AShooterPlayerController::OnScoreboardPressed);
	InputComponent->BindAction(TEXT("Scoreboard"), IE_Released, this, &AShooterPlayerController::OnScoreboardReleased);
	InputComponent->BindAction(TEXT("Menu"), IE_Pressed, this, &AShooterPlayerController::OnMenuPressed);
}

void AShooterPlayerController::OnScoreboardPressed()
{
	bShowScoreboard = true;
}

void AShooterPlayerController::OnScoreboardReleased()
{
	bShowScoreboard = false;
}

void AShooterPlayerController::OnMenuPressed()
{
	// P19 brings the menus (buy menu, scoreboard, game menu).
	UE_LOG(LogShooter, Log, TEXT("Menu (P19)"));
}

void AShooterPlayerController::ViewFrom(float X, float Y, float Z, float Pitch, float Yaw)
{
	UWorld* World = GetWorld();
	if (World == nullptr || PlayerCameraManager == nullptr)
	{
		return;
	}
	if (DebugCamera == nullptr || DebugCamera->IsPendingKillPending())
	{
		FActorSpawnParameters SpawnInfo;
		SpawnInfo.ObjectFlags |= RF_Transient;
		DebugCamera = World->SpawnActor<ACameraActor>(FVector(X, Y, Z), FRotator::ZeroRotator, SpawnInfo);
	}
	if (DebugCamera == nullptr)
	{
		return;
	}
	UCameraComponent* Camera = DebugCamera->GetCameraComponent();
	Camera->SetMode(ECameraMode::FreeLook);
	Camera->SetEyeLocation(FVector(X, Y, Z));
	Camera->SetViewRotation(FRotator(Pitch, Yaw, 0.0f));
	PlayerCameraManager->SetViewTarget(DebugCamera);
	UE_LOG(LogShooter, Log, TEXT("ViewFrom (%.0f, %.0f, %.0f) pitch %.0f yaw %.0f"), static_cast<double>(X),
		static_cast<double>(Y), static_cast<double>(Z), static_cast<double>(Pitch), static_cast<double>(Yaw));
}

void AShooterPlayerController::ViewPawn()
{
	if (PlayerCameraManager != nullptr)
	{
		PlayerCameraManager->SetViewTarget(nullptr);
	}
}

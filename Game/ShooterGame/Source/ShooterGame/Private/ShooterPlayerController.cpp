#include "ShooterPlayerController.h"

#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/InputComponent.h"
#include "Engine/World.h"
#include "ShooterCharacter.h"
#include "ShooterGame.h"
#include "ShooterGameMode.h"
#include "Weapons/ShooterWeapon.h"

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
	InputComponent->BindAction(TEXT("BuyMenu"), IE_Pressed, this, &AShooterPlayerController::OnBuyMenuPressed);
	InputComponent->BindAction(TEXT("MenuItem1"), IE_Pressed, this, &AShooterPlayerController::OnBuyMenuItem1);
	InputComponent->BindAction(TEXT("MenuItem2"), IE_Pressed, this, &AShooterPlayerController::OnBuyMenuItem2);
	InputComponent->BindAction(TEXT("MenuItem3"), IE_Pressed, this, &AShooterPlayerController::OnBuyMenuItem3);
	InputComponent->BindAction(TEXT("MenuItem4"), IE_Pressed, this, &AShooterPlayerController::OnBuyMenuItem4);
	InputComponent->BindAction(TEXT("MenuItem5"), IE_Pressed, this, &AShooterPlayerController::OnBuyMenuItem5);
	InputComponent->BindAction(TEXT("MenuItem6"), IE_Pressed, this, &AShooterPlayerController::OnBuyMenuItem6);
	InputComponent->BindAction(TEXT("MenuItem7"), IE_Pressed, this, &AShooterPlayerController::OnBuyMenuItem7);
}

const TArray<FString>& AShooterPlayerController::GetBuyMenuItems()
{
	static const TArray<FString> Items = {
		TEXT("usp"), TEXT("ak47"), TEXT("awp"), TEXT("hegrenade"), TEXT("vest"), TEXT("vesthelm"), TEXT("defuser")};
	return Items;
}

void AShooterPlayerController::SetBuyMenuOpen(bool bOpen)
{
	bBuyMenuOpen = bOpen;
	if (bOpen)
	{
		LastBuyMessage.Empty();
	}
}

void AShooterPlayerController::OnBuyMenuPressed()
{
	SetBuyMenuOpen(!bBuyMenuOpen);
}

void AShooterPlayerController::OnBuyMenuItem(int32 Number)
{
	if (bBuyMenuOpen && GetBuyMenuItems().IsValidIndex(Number - 1))
	{
		Buy(GetBuyMenuItems()[Number - 1]);
	}
}

void AShooterPlayerController::OnBuyMenuItem1()
{
	OnBuyMenuItem(1);
}

void AShooterPlayerController::OnBuyMenuItem2()
{
	OnBuyMenuItem(2);
}

void AShooterPlayerController::OnBuyMenuItem3()
{
	OnBuyMenuItem(3);
}

void AShooterPlayerController::OnBuyMenuItem4()
{
	OnBuyMenuItem(4);
}

void AShooterPlayerController::OnBuyMenuItem5()
{
	OnBuyMenuItem(5);
}

void AShooterPlayerController::OnBuyMenuItem6()
{
	OnBuyMenuItem(6);
}

void AShooterPlayerController::OnBuyMenuItem7()
{
	OnBuyMenuItem(7);
}

void AShooterPlayerController::Buy(FString Item)
{
	UWorld* World = GetWorld();
	AShooterGameMode* GameMode = World != nullptr ? World->GetAuthGameMode<AShooterGameMode>() : nullptr;
	AShooterCharacter* ShooterPawn = Cast<AShooterCharacter>(GetPawn());
	if (GameMode == nullptr || ShooterPawn == nullptr)
	{
		LastBuyMessage = TEXT("You cannot buy now");
		return;
	}
	FString Reason;
	LastBuyMessage = GameMode->Buy(ShooterPawn, Item, &Reason) ? FString::Printf(TEXT("Bought %s"), *Item)
															   : FString::Printf(TEXT("%s: %s"), *Item, *Reason);
	UE_LOG(LogShooter, Log, TEXT("Buy %s: %s"), *Item, *LastBuyMessage);
}

void AShooterPlayerController::Give(FString WeaponName)
{
	AShooterCharacter* ShooterPawn = Cast<AShooterCharacter>(GetPawn());
	UClass* WeaponClass = AShooterWeapon::FindWeaponClass(WeaponName);
	if (ShooterPawn == nullptr || WeaponClass == nullptr)
	{
		UE_LOG(LogShooter, Warning, TEXT("give: no weapon '%s'"), *WeaponName);
		return;
	}
	ShooterPawn->EquipWeapon(ShooterPawn->GiveWeapon(WeaponClass));
}

void AShooterPlayerController::God()
{
	if (AShooterCharacter* ShooterPawn = Cast<AShooterCharacter>(GetPawn()))
	{
		ShooterPawn->SetGodMode(!ShooterPawn->IsGodMode());
		UE_LOG(LogShooter, Log, TEXT("god mode %s"), ShooterPawn->IsGodMode() ? TEXT("ON") : TEXT("OFF"));
	}
}

void AShooterPlayerController::Kill()
{
	if (AShooterCharacter* ShooterPawn = Cast<AShooterCharacter>(GetPawn()))
	{
		ShooterPawn->Suicide();
	}
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
	// Escape closes the buy menu (ShooterGame has no pause menu).
	if (bBuyMenuOpen)
	{
		SetBuyMenuOpen(false);
	}
}

void AShooterPlayerController::NotifyHitConfirmed(bool bHeadshot, bool bKilled)
{
	const UWorld* World = GetWorld();
	LastHitTime = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	bLastHitHeadshot = bHeadshot;
	bLastHitKill = bKilled;
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

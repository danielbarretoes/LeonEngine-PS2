#include "ShooterPlayerController_Menu.h"

#include "GameFramework/HUD.h"
#include "UI/ShooterMainMenuWidget.h"

AShooterPlayerController_Menu::AShooterPlayerController_Menu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void AShooterPlayerController_Menu::BeginPlay()
{
	Super::BeginPlay();
	BaseYaw = GetControlRotation().Yaw;
	if (IsLocalController() && MyHUD != nullptr)
	{
		MainMenu = MyHUD->AddWidget<UShooterMainMenuWidget>();
		MainMenu->SetShown(true);
		SetInputMode(FInputModeUIOnly());
	}
}

void AShooterPlayerController_Menu::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	// A slow look from side to side over the backdrop.
	SwayTime += DeltaTime;
	const float Phase = SwayPeriod > 0.0f ? (2.0f * PI * SwayTime) / SwayPeriod : 0.0f;
	FRotator View = GetControlRotation();
	View.Yaw = BaseYaw + (SwayYaw * FMath::Sin(Phase));
	SetControlRotation(View);
}

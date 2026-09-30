#include "ShooterGame_Menu.h"

#include "GameFramework/HUD.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ShooterGame.h"
#include "ShooterPlayerController_Menu.h"

AShooterGame_Menu::AShooterGame_Menu(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PlayerControllerClass = AShooterPlayerController_Menu::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = AHUD::StaticClass();
}

void AShooterGame_Menu::StartPlay()
{
	Super::StartPlay();
	if (FParse::Param(FCommandLine::Get(), TEXT("botmatch")))
	{
		UE_LOG(LogShooter, Display, TEXT("Botmatch: the menu travels to %s"), *BotMatchMapName);
		UGameplayStatics::OpenLevel(this, FName(*BotMatchMapName));
	}
}

void AShooterGame_Menu::RestartPlayer(AController* /*NewPlayer*/)
{
}

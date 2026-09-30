#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ShooterGame_Menu.generated.h"

/**
 * The main menu's game mode (ps2-polish P9; UE ShooterGame's AShooterGame_Menu), the MainMenu map's through
 * DefaultEngine.ini's GameModeMapPrefixes: no pawn (the camera stands at the map's player start), the menu's player
 * controller (AShooterPlayerController_Menu, which shows UShooterMainMenuWidget) and a plain HUD.
 *
 * A bot match (`-botmatch`, BotMatch.bat and MeasurePS2 start one without a map) skips the menu: StartPlay travels to
 * BotMatchMapName at once.
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterGame_Menu : public AGameModeBase
{
	GENERATED_BODY()

public:
	AShooterGame_Menu(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The map a bot match plays (`-botmatch` without a map on the command line). */
	UPROPERTY(Config)
	FString BotMatchMapName = TEXT("/Game/Maps/de_leon");

	/** A bot match travels to its map. */
	void StartPlay() override;
	/** Nobody spawns in the menu (UE ShooterGame: "don't restart"). */
	void RestartPlayer(AController* NewPlayer) override;
};

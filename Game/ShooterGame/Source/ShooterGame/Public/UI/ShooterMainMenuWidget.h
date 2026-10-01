#pragma once

#include "CoreMinimal.h"
#include "ShooterTypes.h"
#include "UI/ShooterMenuWidget.h"
#include "ShooterMainMenuWidget.generated.h"

/**
 * The main menu (ps2-polish P9; UE ShooterGame's FShooterMainMenu), the MainMenu map's (AShooterPlayerController_Menu
 * shows it): the match to play and the options.
 *
 * - Map: the project's maps (MapNames, DefaultGame.ini: de_leon and de_puerto), shown by their short names.
 * - Difficulty: Easy, Normal, Hard, Expert (the bots' presets, AShooterAIController::DifficultyPresets).
 * - Rounds to win: 3 (a best of 5), 5, 8 or 16 (MaxRounds = 2 N - 1).
 * - Bots: 1 to 9 (ten players at most, the PS2's budget), shared out around the player's team.
 * - Options (FShooterOptionsPage), Start, and Quit (Win64 only: a console game is switched off).
 *
 * The choices come from the player's options (UShooterPersistentUser::GetMatchSettings) and Start saves them there,
 * then travels to the map with the match's URL options (UGameplayStatics::OpenLevel, FShooterMatchSettings::
 * GetURLOptions); the match asks for the team (UShooterTeamMenuWidget).
 */
UCLASS(Config = Game)
class SHOOTERGAME_API UShooterMainMenuWidget : public UShooterMenuWidget
{
	GENERATED_BODY()

public:
	UShooterMainMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The maps the menu offers, long package names (`/Game/Maps/de_leon`). */
	UPROPERTY(Config)
	TArray<FString> MapNames;

	/** Takes the player's last match (its options) when shown. */
	void SetShown(bool bShown) override;

	/** The match the menu would start. */
	[[nodiscard]] const FShooterMatchSettings& GetMatchSettings() const
	{
		return Settings;
	}
	/** Steps a line's value (the lines' OnValueStep; the tests). */
	void StepMap(int32 Direction);
	void StepDifficulty(int32 Direction);
	void StepRoundsToWin(int32 Direction);
	void StepBots(int32 Direction);
	/** Saves the choices and travels to the match (the Start line). */
	void StartMatch();

	/** The lines (the tests). */
	[[nodiscard]] UShooterMenuButton* GetStartButton() const
	{
		return StartButton;
	}
	[[nodiscard]] UShooterMenuButton* GetBotsButton() const
	{
		return BotsButton;
	}

protected:
	void BuildPages() override;
	void OnBack() override;

private:
	/** Shows the choices' values. */
	void Refresh();
	/** The map's short name ("de_leon"). */
	[[nodiscard]] static FString GetMapDisplayName(const FString& MapName);
	/** Leaves the game (Win64's Quit). */
	void QuitGame();

	FShooterMatchSettings Settings;

	UPROPERTY()
	UShooterMenuButton* MapButton = nullptr;
	UPROPERTY()
	UShooterMenuButton* DifficultyButton = nullptr;
	UPROPERTY()
	UShooterMenuButton* RoundsButton = nullptr;
	UPROPERTY()
	UShooterMenuButton* BotsButton = nullptr;
	UPROPERTY()
	UShooterMenuButton* StartButton = nullptr;
};

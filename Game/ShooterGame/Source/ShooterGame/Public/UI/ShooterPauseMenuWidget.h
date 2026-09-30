#pragma once

#include "CoreMinimal.h"
#include "UI/ShooterMenuWidget.h"
#include "ShooterPauseMenuWidget.generated.h"

class UTextBlock;

/**
 * The pause menu (ps2-polish P9; UE ShooterGame's in-game menu), on Escape or Start during a match: Resume, Change
 * team (the team menu), Options (FShooterOptionsPage) and Quit to main menu, over a shade of the paused game, with the
 * match's state (the map, the score, the round, the rounds to win). The player controller pauses the world while it
 * shows (AShooterPlayerController::ShowPauseMenu: UE's SetPause). Start, or Back on its first page, resumes.
 */
UCLASS()
class SHOOTERGAME_API UShooterPauseMenuWidget : public UShooterMenuWidget
{
	GENERATED_BODY()

public:
	UShooterPauseMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Keeps the match's line up to date. */
	void NativeTick(float DeltaTime) override;

	/** The lines (the tests). */
	[[nodiscard]] UShooterMenuButton* GetResumeButton() const
	{
		return ResumeButton;
	}
	[[nodiscard]] UShooterMenuButton* GetChangeTeamButton() const
	{
		return ChangeTeamButton;
	}
	[[nodiscard]] UShooterMenuButton* GetQuitButton() const
	{
		return QuitButton;
	}
	/** The match's line (the tests). */
	[[nodiscard]] const FString& GetStatusText() const
	{
		return ShownStatus;
	}

protected:
	void BuildPages() override;
	void OnBack() override;
	/** Start resumes from any page. */
	FReply HandleMenuKey(const FKeyEvent& InKeyEvent) override;

private:
	void Resume();

	FString ShownStatus;

	UPROPERTY()
	UTextBlock* StatusText = nullptr;
	UPROPERTY()
	UShooterMenuButton* ResumeButton = nullptr;
	UPROPERTY()
	UShooterMenuButton* ChangeTeamButton = nullptr;
	UPROPERTY()
	UShooterMenuButton* QuitButton = nullptr;
};

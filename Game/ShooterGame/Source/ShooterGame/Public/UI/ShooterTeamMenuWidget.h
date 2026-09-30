#pragma once

#include "CoreMinimal.h"
#include "ShooterTypes.h"
#include "UI/ShooterMenuWidget.h"
#include "ShooterTeamMenuWidget.generated.h"

/**
 * The team menu (ps2-polish P9; CS's "Select a team"): Counter-Terrorists, Terrorists (each with its players now),
 * Auto-select (the smaller team) and Spectate. The player controller shows it when the player joins a match without
 * `?team=` (the game waits in its warmup: AShooterGameMode::IsChoosingTeam) and from the pause menu's Change team
 * (CS's rule: the change takes effect at the next round, the player dying if alive). A choice goes to
 * AShooterPlayerController::JoinTeam. Back (Escape, Circle) opens the pause menu on the first choice (the way back to
 * the main menu) and returns to it on a change (Cancel too).
 */
UCLASS()
class SHOOTERGAME_API UShooterTeamMenuWidget : public UShooterMenuWidget
{
	GENERATED_BODY()

public:
	UShooterTeamMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Keeps the teams' player counts on their lines. */
	void NativeTick(float DeltaTime) override;

	/** The first choice (no Cancel; Back opens the pause menu) or a change (Cancel returns to the pause menu). */
	void SetInitialChoice(bool bInitial);
	[[nodiscard]] bool IsInitialChoice() const
	{
		return bInitialChoice;
	}

	/** A choice's line (the tests). */
	[[nodiscard]] UShooterMenuButton* GetChoiceButton(EShooterTeamChoice Choice) const;

protected:
	void BuildPages() override;
	void OnBack() override;

private:
	void Choose(EShooterTeamChoice Choice);

	bool bInitialChoice = true;
	/** The counts on the team lines (set again when they change). */
	int32 ShownCT = -1;
	int32 ShownT = -1;

	UPROPERTY()
	TArray<UShooterMenuButton*> ChoiceButtons;
	UPROPERTY()
	UShooterMenuButton* CancelButton = nullptr;
};

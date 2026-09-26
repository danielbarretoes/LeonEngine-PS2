#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShooterPlayerController.generated.h"

class ACameraActor;

/**
 * The human player's controller (UE ShooterGame: AShooterPlayerController): the scoreboard key (Tab, held) and the
 * menu key (Esc), placeholders until P19's scoreboard and buy menu, and a debug view for captures.
 */
UCLASS()
class SHOOTERGAME_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AShooterPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Binds Scoreboard (Tab) and Menu (Esc) on the controller's input component (UE: SetupInputComponent). */
	void SetupInputComponent() override;

	/** Tab is held: the HUD lists the players by team (AShooterHUD). */
	[[nodiscard]] bool IsScoreboardShown() const
	{
		return bShowScoreboard;
	}

	/**
	 * Debug: views the map from a point, looking along Pitch / Yaw, through a camera actor (the view target) until
	 * ViewPawn: `-ExecCmds="ViewFrom 0 0 4000 -89 0"` for a top-down capture.
	 */
	UFUNCTION(Exec)
	void ViewFrom(float X, float Y, float Z, float Pitch, float Yaw);

	/** Debug: back to the pawn's view. */
	UFUNCTION(Exec)
	void ViewPawn();

private:
	void OnScoreboardPressed();
	void OnScoreboardReleased();
	/** Esc: the game menu (P19); logs for now. */
	void OnMenuPressed();

	bool bShowScoreboard = false;

	/** The camera of ViewFrom. */
	UPROPERTY(Transient)
	ACameraActor* DebugCamera = nullptr;
};

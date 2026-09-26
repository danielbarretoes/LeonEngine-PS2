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

	/**
	 * A shot of this player hurt a character (UE ShooterGame: the HUD's hit notify; CS's hit sound): the HUD draws the
	 * hit marker for HitMarkerDuration, red for a kill.
	 */
	void NotifyHitConfirmed(bool bHeadshot, bool bKilled);
	/** The world time of the last confirmed hit (negative before the first), and what it was. */
	[[nodiscard]] float GetLastHitTime() const
	{
		return LastHitTime;
	}
	[[nodiscard]] bool WasLastHitHeadshot() const
	{
		return bLastHitHeadshot;
	}
	[[nodiscard]] bool WasLastHitKill() const
	{
		return bLastHitKill;
	}
	/** How many hits have been confirmed. */
	[[nodiscard]] int32 GetNumHitsConfirmed() const
	{
		return NumHitsConfirmed;
	}

	/** Debug: back to the pawn's view. */
	UFUNCTION(Exec)
	void ViewPawn();

private:
	void OnScoreboardPressed();
	void OnScoreboardReleased();
	/** Esc: the game menu (P19); logs for now. */
	void OnMenuPressed();

	bool bShowScoreboard = false;

	float LastHitTime = -1.0f;
	bool bLastHitHeadshot = false;
	bool bLastHitKill = false;
	int32 NumHitsConfirmed = 0;

	/** The camera of ViewFrom. */
	UPROPERTY(Transient)
	ACameraActor* DebugCamera = nullptr;
};

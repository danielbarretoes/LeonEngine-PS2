#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "ShooterPlayerController.generated.h"

class ACameraActor;

/**
 * The human player's controller (UE ShooterGame: AShooterPlayerController): the scoreboard (Tab, held), the buy menu
 * (B; its items on the number keys while it is open, Esc closes it), the hit marker's state, CS's console commands and
 * a debug view for captures.
 *
 * Console (Exec): `Buy <item>` (usp, ak47, awp, hegrenade, vest, vesthelm, defuser: AShooterGameMode::Buy), `buymenu`
 * (opens or closes the menu, as B), and the cheats `give <weapon>` (a weapon by name, free, anywhere), `god` (no
 * damage, toggles) and `kill` (suicide).
 */
UCLASS()
class SHOOTERGAME_API AShooterPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AShooterPlayerController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Binds Scoreboard (Tab) and Menu (Esc) on the controller's input component (UE: SetupInputComponent). */
	void SetupInputComponent() override;

	/** The buy menu's items, in the order of their number keys (CS's buy menu, flattened). */
	static const TArray<FString>& GetBuyMenuItems();

	/** The buy menu is open (B): the HUD draws it and the number keys buy. */
	[[nodiscard]] bool IsBuyMenuOpen() const
	{
		return bBuyMenuOpen;
	}
	void SetBuyMenuOpen(bool bOpen);
	/** The last buy's result, for the HUD ("bought ak47", "not enough money"). */
	[[nodiscard]] const FString& GetLastBuyMessage() const
	{
		return LastBuyMessage;
	}

	/** Buys an item for the pawn (AShooterGameMode::Buy). */
	UFUNCTION(Exec)
	void Buy(FString Item);

	/** Opens or closes the buy menu (CS: buymenu). */
	UFUNCTION(Exec)
	void BuyMenu();

	/** Cheat: a weapon by name, free (CS: give weapon_ak47). */
	UFUNCTION(Exec)
	void Give(FString WeaponName);

	/** Cheat: the pawn takes no damage; again to take it. */
	UFUNCTION(Exec)
	void God();

	/** The pawn dies (CS: kill). */
	UFUNCTION(Exec)
	void Kill();

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

	/** Debug: back to the pawn's view. */
	UFUNCTION(Exec)
	void ViewPawn();

private:
	void OnScoreboardPressed();
	void OnScoreboardReleased();
	void OnBuyMenuPressed();
	/** A number key while the buy menu is open: buys that item (1-based). */
	void OnBuyMenuItem(int32 Number);
	void OnBuyMenuItem1();
	void OnBuyMenuItem2();
	void OnBuyMenuItem3();
	void OnBuyMenuItem4();
	void OnBuyMenuItem5();
	void OnBuyMenuItem6();
	void OnBuyMenuItem7();
	/** Esc: closes the buy menu. */
	void OnMenuPressed();

	bool bShowScoreboard = false;
	bool bBuyMenuOpen = false;
	FString LastBuyMessage;

	float LastHitTime = -1.0f;
	bool bLastHitHeadshot = false;
	bool bLastHitKill = false;

	/** The camera of ViewFrom. */
	UPROPERTY(Transient)
	ACameraActor* DebugCamera = nullptr;
};

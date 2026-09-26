#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ShooterHUD.generated.h"

class AShooterCharacter;
class AShooterGameState;
class AShooterPlayerController;

/**
 * ShooterGame's HUD (UE ShooterGame: AShooterHUD), Counter-Strike's layout:
 * - the crosshair at the centre, its gap growing with the weapon's spread (the dynamic crosshair), and the hit marker
 *   (four diagonal ticks, red on a kill) for HitMarkerDuration after a confirmed hit; the AWP's scope (a square view
 *   between black side bars, thin black cross lines) instead of the crosshair while zoomed;
 * - bottom left the health, the armor (H: a helmet) and the money, bottom right the weapon with its clip and reserve,
 *   the bomb (C4) and the defuse kit when carried;
 * - top centre the round's clock (the freeze, then the round's time; the bomb once planted) and the score; top right
 *   the kill feed (the last kills, for KillFeedDuration);
 * - the centre's messages (the warmup, the round's result, the match's end) and a bar while planting or defusing;
 * - the scoreboard while Tab is held (each team's players: kills, deaths, money, dead ones marked);
 * - the buy menu (UShooterBuyMenuWidget, a UMG widget the HUD owns) while it is open.
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterHUD : public AHUD
{
	GENERATED_BODY()

public:
	AShooterHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Adds the buy menu widget. */
	void BeginPlay() override;
	/** Draws the HUD into Canvas (UE: DrawHUD); the widgets paint after it. */
	void DrawHUD() override;

	/** The crosshair's colour (CS's default green). */
	UPROPERTY(Config)
	FLinearColor CrosshairColor = FLinearColor(0.0f, 1.0f, 0.0f);

	/** The gap between the centre and each arm with no spread, pixels. */
	UPROPERTY(Config)
	float CrosshairGap = 4.0f;

	/** The length of each arm, pixels. */
	UPROPERTY(Config)
	float CrosshairLength = 7.0f;

	/** The thickness of the arms, pixels. */
	UPROPERTY(Config)
	float CrosshairThickness = 2.0f;

	/** How much the weapon's spread opens the crosshair: the spread's pixels at the screen times this. */
	UPROPERTY(Config)
	float CrosshairSpreadScale = 1.0f;

	/** Seconds the hit marker shows after a hit. */
	UPROPERTY(Config)
	float HitMarkerDuration = 0.25f;

	/** Seconds a kill stays in the kill feed. */
	UPROPERTY(Config)
	float KillFeedDuration = 6.0f;

	/** The status text's colour (CS's amber). */
	UPROPERTY(Config)
	FLinearColor StatusColor = FLinearColor(1.0f, 0.75f, 0.2f);

	/** The pawn the HUD shows (the owner's), or null. */
	[[nodiscard]] AShooterCharacter* GetViewedPawn() const;
	/** The world's game state, as the game's class, or null. */
	[[nodiscard]] AShooterGameState* GetShooterGameState() const;
	/** The crosshair's gap now: CrosshairGap plus the drawn weapon's spread on screen, pixels. */
	[[nodiscard]] float GetCrosshairGap() const;
	/** The owner as the game's controller. */
	[[nodiscard]] AShooterPlayerController* GetShooterPlayerController() const;

private:
	/** The health, the armor, the money and the weapon's ammunition. */
	void DrawStatus();
	/** The round's clock and the score. */
	void DrawRoundInfo();
	/** The last kills. */
	void DrawKillFeed();
	/** The warmup, the round's result, the match's end, the bomb. */
	void DrawMessages();
	/** A bar under the crosshair while planting or defusing. */
	void DrawProgress();
	/** The ticks of a confirmed hit. */
	void DrawHitMarker();
	/** The sniper's scope: true when drawn (the crosshair is not). */
	bool DrawScope();
	/** The crosshair: four arms around the centre. */
	void DrawCrosshair();
	/** The players by team (Tab held). */
	void DrawScoreboard();
	[[nodiscard]] float GetWorldTime() const;
};

/**
 * The buy menu (CS's, flattened into one list): each item of AShooterPlayerController::GetBuyMenuItems on its number
 * key, with its price; items the player cannot buy now are grey. The money and why buying is refused (outside a buy
 * zone, after the buy time) head it, and the last buy's result follows it. Drawn while the owner's menu is open.
 */
UCLASS()
class SHOOTERGAME_API UShooterBuyMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UShooterBuyMenuWidget(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	void NativePaint(FPaintContext& Ctx) override;
};

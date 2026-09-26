#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ShooterHUD.generated.h"

/**
 * ShooterGame's HUD (UE ShooterGame: AShooterHUD): Counter-Strike's crosshair at the centre of the screen; the health
 * and the armor (bottom left) and the weapon with its clip and reserve (bottom right) of the viewed pawn; the hit
 * marker (four diagonal ticks around the crosshair, red on a kill) for HitMarkerDuration after a confirmed hit; the
 * sniper's scope (a square view between black side bars, with thin black cross lines) instead of the crosshair while
 * zoomed; and, while the scoreboard key is held, the players by team (P19 draws the real scoreboard, the money, the
 * time and the kill feed).
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterHUD : public AHUD
{
	GENERATED_BODY()

public:
	AShooterHUD(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Draws the crosshair (and the scoreboard placeholder) into Canvas (UE: DrawHUD). */
	void DrawHUD() override;

	/** The crosshair's colour (CS's default green). */
	UPROPERTY(Config)
	FLinearColor CrosshairColor = FLinearColor(0.0f, 1.0f, 0.0f);

	/** The gap between the centre and each arm, pixels. */
	UPROPERTY(Config)
	float CrosshairGap = 4.0f;

	/** The length of each arm, pixels. */
	UPROPERTY(Config)
	float CrosshairLength = 7.0f;

	/** The thickness of the arms, pixels. */
	UPROPERTY(Config)
	float CrosshairThickness = 2.0f;

	/** Seconds the hit marker shows after a hit. */
	UPROPERTY(Config)
	float HitMarkerDuration = 0.25f;

	/** The status text's colour (CS's amber). */
	UPROPERTY(Config)
	FLinearColor StatusColor = FLinearColor(1.0f, 0.75f, 0.2f);

private:
	/** The health, the armor and the weapon's ammunition. */
	void DrawStatus();
	/** The ticks of a confirmed hit. */
	void DrawHitMarker();
	/** The sniper's scope: true when drawn (the crosshair is not). */
	bool DrawScope();
	/** The crosshair: four arms around the centre. */
	void DrawCrosshair();
	/** The players by team (Tab held). */
	void DrawScoreboard();
};

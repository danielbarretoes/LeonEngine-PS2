#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "ShooterHUD.generated.h"

/**
 * ShooterGame's HUD (UE ShooterGame: AShooterHUD): Counter-Strike's crosshair at the centre of the screen and, while
 * the scoreboard key is held, the players by team (P19 draws the real scoreboard, the money, the time and the
 * kill feed).
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

private:
	/** The crosshair: four arms around the centre. */
	void DrawCrosshair();
	/** The players by team (Tab held). */
	void DrawScoreboard();
};

#pragma once

#include "CoreMinimal.h"
#include "ShooterPlayerController.h"
#include "ShooterPlayerController_Menu.generated.h"

class UShooterMainMenuWidget;

/**
 * The main menu's player controller (ps2-polish P9; UE ShooterGame's AShooterPlayerController_Menu): it has the
 * player's options as in a match (AShooterPlayerController: loaded from the memory card, the options page changes
 * them) and shows the main menu (UShooterMainMenuWidget) on its HUD, the mouse free on Win64. Without a pawn the camera
 * looks from the map's player start, swaying slowly over the backdrop.
 */
UCLASS()
class SHOOTERGAME_API AShooterPlayerController_Menu : public AShooterPlayerController
{
	GENERATED_BODY()

public:
	AShooterPlayerController_Menu(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The main menu (the tests). */
	[[nodiscard]] UShooterMainMenuWidget* GetMainMenu() const
	{
		return MainMenu;
	}

	/** The camera's sway: degrees to each side and seconds for a full sway. */
	UPROPERTY()
	float SwayYaw = 6.0f;
	UPROPERTY()
	float SwayPeriod = 40.0f;

	void PlayerTick(float DeltaTime) override;

protected:
	/** Shows the main menu. */
	void BeginPlay() override;

private:
	UPROPERTY(Transient)
	UShooterMainMenuWidget* MainMenu = nullptr;
	/** The view's yaw at the start, and the time swayed. */
	float BaseYaw = 0.0f;
	float SwayTime = 0.0f;
};

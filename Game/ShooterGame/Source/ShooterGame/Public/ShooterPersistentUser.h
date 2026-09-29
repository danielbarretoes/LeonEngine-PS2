#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ShooterPersistentUser.generated.h"

class AShooterPlayerController;

/**
 * The player's options, kept between sessions (UE ShooterGame: UShooterPersistentUser; Docs/PLANS/ps2-shipping.md N24):
 * the aim sensitivity (a scale of the mouse's degrees a pixel and of the right stick's turn rates), an inverted Y axis
 * (the mouse and the stick), the sound's volume and the crosshair's colour. They live in the save slot "Settings":
 * Saved/SaveGames/Settings.sav on the desktop, the memory card's ShooterGame folder on the PS2
 * (UGameplayStatics::SaveGameToSlot). The player controller loads them when its player plays at a screen, applies them,
 * and saves them when a console command changes one (SetSensitivity, SetInvertY, SetVolume, SetCrosshairColor).
 */
UCLASS()
class SHOOTERGAME_API UShooterPersistentUser : public USaveGame
{
	GENERATED_BODY()

public:
	UShooterPersistentUser(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The save slot of the options. */
	static const TCHAR* GetSlotName()
	{
		return TEXT("Settings");
	}

	/** The user's options from the slot, or a new set of defaults when there is none (or it cannot be read). */
	static UShooterPersistentUser* LoadPersistentUser(int32 UserIndex);

	/** Writes the options to the slot; false (logged with the reason) when it cannot. */
	bool SaveToSlot(int32 UserIndex);

	/** The aim's scale: 1 is the game's (0.07° a mouse pixel, BaseTurnRate / BaseLookUpRate on the stick). */
	UPROPERTY()
	float AimSensitivity = 1.0f;

	/** Up on the mouse or the stick looks down. */
	UPROPERTY()
	bool bInvertedYAxis = false;

	/** The sound's volume, 0 to 1 (the audio device's master volume). */
	UPROPERTY()
	float SoundVolume = 1.0f;

	/** The crosshair's colour (AShooterHUD::CrosshairColor). */
	UPROPERTY()
	FLinearColor CrosshairColor = FLinearColor(0.0f, 1.0f, 0.0f);
};

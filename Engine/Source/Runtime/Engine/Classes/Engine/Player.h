#pragma once

#include "CoreMinimal.h"
#include "Misc/Exec.h"
#include "UObject/Object.h"
#include "Player.generated.h"

class APlayerController;
class UWorld;

/**
 * Someone playing the game (UE: UPlayer): the player controller that logged in for it. ULocalPlayer is the player at
 * this machine; Leon has no network players.
 */
UCLASS(Transient, Config = Engine)
class ENGINE_API UPlayer
	: public UObject
	, public FExec
{
	GENERATED_BODY()

public:
	/** The controller this player logged in with (UE: PlayerController); cleared when the map changes. */
	UPROPERTY(Transient)
	APlayerController* PlayerController = nullptr;

	/** The player's controller in InWorld, or null (UE: GetPlayerController). */
	[[nodiscard]] APlayerController* GetPlayerController(const UWorld* InWorld) const;

	/**
	 * The player's console commands (UE: UPlayer::Exec): offered to the player input, the controller, its pawn, the
	 * game mode, the game state and the world settings, each through ProcessConsoleExec (their Exec UFUNCTIONs).
	 */
	bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/** Runs one or more commands separated by '|' through Exec and returns what they printed (UE: ConsoleCommand). */
	FString ConsoleCommand(const FString& Cmd, bool bWriteToLog = true);
};

#pragma once

#include "CoreMinimal.h"
#include "Engine/Player.h"
#include "LocalPlayer.generated.h"

class UGameInstance;

/**
 * The player at this machine (UE: ULocalPlayer), created by the game instance (CreateInitialPlayer) inside the engine.
 * When a map loads it logs in (SpawnPlayActor → UWorld::SpawnPlayActor → AGameModeBase::Login / PostLogin) and keeps
 * the controller it got.
 */
UCLASS(Transient, Config = Engine)
class ENGINE_API ULocalPlayer : public UPlayer
{
	GENERATED_BODY()

public:
	/**
	 * Logs in to InWorld with the URL's options and the player's name (UE: SpawnPlayActor); false with OutError when
	 * the game mode refused.
	 */
	virtual bool SpawnPlayActor(const FString& URL, FString& OutError, UWorld* InWorld);

	/** Joined the game instance (UE: PlayerAdded). */
	virtual void PlayerAdded(int32 InControllerId);

	/** Left the game instance (UE: PlayerRemoved). */
	virtual void PlayerRemoved();

	/** The player's name in the URL (UE: GetNickname); empty, so the game mode names the player. */
	[[nodiscard]] virtual FString GetNickname() const;

	/** Extra login options (UE: GetGameLoginOptions). */
	[[nodiscard]] virtual FString GetGameLoginOptions() const;

	/** The controller id of the input device the player uses (UE). */
	[[nodiscard]] int32 GetControllerId() const
	{
		return ControllerId;
	}
	void SetControllerId(int32 NewControllerId)
	{
		ControllerId = NewControllerId;
	}

	/** The game instance that owns the player (UE: GetGameInstance). */
	[[nodiscard]] UGameInstance* GetGameInstance() const;

	/** The game instance's world (UE: GetWorld). */
	[[nodiscard]] UWorld* GetWorld() const;

	/** The local player's commands, then the player's (UE: ULocalPlayer::Exec). */
	bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

private:
	/** UE: ControllerId. */
	int32 ControllerId = 0;

	/** Leon: the game instance the player was added to (UE finds it through the viewport client). */
	UPROPERTY(Transient)
	UGameInstance* OwningGameInstance = nullptr;

	friend class UGameInstance;
};

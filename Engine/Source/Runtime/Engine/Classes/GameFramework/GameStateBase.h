#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Info.h"
#include "GameStateBase.generated.h"

class APlayerState;

/**
 * Shared match/session state (UE: AGameStateBase), an AInfo the game mode spawns (GameStateClass) and the world
 * points at (UWorld::GetGameState).
 *
 * Leon keeps a simple match clock here: HandleMatchHasStarted / HandleMatchHasEnded record the world's time, and
 * GetServerWorldTimeSeconds gives the seconds in between (it does not tick: AInfo actors do not). Begin play starts it
 * (HandleBeginPlay). AGameState adds UE's MatchState.
 */
UCLASS()
class ENGINE_API AGameStateBase : public AInfo
{
	GENERATED_BODY()

public:
	AGameStateBase(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** Resets the match clock and flags, not PlayerArray (UE: logout removes players). */
	virtual void Reset()
	{
		MatchStartSeconds = 0.0f;
		MatchEndSeconds = 0.0f;
		bMatchInProgress = false;
		bMatchHasEnded = false;
	}

	/** Unreal HasMatchStarted. */
	[[nodiscard]] bool HasMatchStarted() const
	{
		return bMatchInProgress;
	}
	/** Unreal HasMatchEnded. */
	[[nodiscard]] bool HasMatchEnded() const
	{
		return bMatchHasEnded;
	}
	/**
	 * Seconds the match has been in progress (Leon's GetServerWorldTimeSeconds: from HandleMatchHasStarted to now, or
	 * to HandleMatchHasEnded).
	 */
	[[nodiscard]] float GetServerWorldTimeSeconds() const;

	/** Unreal PlayerArray — PlayerStates registered via PostLogin / Logout. */
	[[nodiscard]] const TArray<APlayerState*>& GetPlayerArray() const
	{
		return PlayerArray;
	}
	/** Unreal PlayerArray.Num(). */
	[[nodiscard]] int32 GetNumPlayers() const
	{
		return PlayerArray.Num();
	}

	/** Unreal AGameStateBase::AddPlayerState (idempotent). */
	void AddPlayerState(APlayerState* PlayerState)
	{
		if (PlayerState == nullptr)
		{
			return;
		}
		PlayerArray.AddUnique(PlayerState);
	}

	/** Unreal AGameStateBase::RemovePlayerState. */
	void RemovePlayerState(APlayerState* PlayerState)
	{
		if (PlayerState == nullptr)
		{
			return;
		}
		PlayerArray.Remove(PlayerState);
	}

	[[nodiscard]] bool HasPlayerState(const APlayerState* PlayerState) const
	{
		if (PlayerState == nullptr)
		{
			return false;
		}
		return PlayerArray.ContainsByPredicate(
			[PlayerState](const APlayerState* Entry) { return Entry == PlayerState; });
	}

	/**
	 * The world began play (UE: HandleBeginPlay, from AGameModeBase::StartPlay). Without match states the match is play
	 * itself, so Leon's clock starts (HandleMatchHasStarted); AGameState starts it when its match is in progress.
	 */
	virtual void HandleBeginPlay()
	{
		MarkHasBegunPlay();
		HandleMatchHasStarted();
	}

	/** True once the world began play (UE: HasBegunPlay). */
	[[nodiscard]] bool HasBegunPlay() const
	{
		return bReplicatedHasBegunPlay;
	}

	/** Authority: mark match in progress (pairs with GameMode::StartMatch). */
	virtual void HandleMatchHasStarted();
	/** Authority: mark match finished (pairs with GameMode::EndMatch). */
	virtual void HandleMatchHasEnded();

protected:
	/** Records that the world began play, without starting the clock (AGameState). */
	void MarkHasBegunPlay()
	{
		bReplicatedHasBegunPlay = true;
	}

private:
	/** The world's time the match started and ended at (Leon; UE: a replicated world time). */
	UPROPERTY()
	float MatchStartSeconds = 0.0f;

	UPROPERTY()
	float MatchEndSeconds = 0.0f;

	UPROPERTY()
	bool bMatchInProgress = false;

	/** The world began play (UE: bReplicatedHasBegunPlay). */
	UPROPERTY()
	bool bReplicatedHasBegunPlay = false;

	UPROPERTY()
	bool bMatchHasEnded = false;

	/** The players' states (UE: PlayerArray). */
	UPROPERTY()
	TArray<APlayerState*> PlayerArray;
};

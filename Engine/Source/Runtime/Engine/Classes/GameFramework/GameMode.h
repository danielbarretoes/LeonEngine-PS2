#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameMode.generated.h"

/** The states of a match (UE: the MatchState namespace of GameMode.h). */
namespace MatchState
{
	/** The world is loading; actors are not ticking yet. */
	extern ENGINE_API const FName EnteringMap;
	/** Actors tick, players have not started yet. */
	extern ENGINE_API const FName WaitingToStart;
	/** The game is being played. */
	extern ENGINE_API const FName InProgress;
	/** The game has ended; players are still in the level. */
	extern ENGINE_API const FName WaitingPostMatch;
	/** The map is being left. */
	extern ENGINE_API const FName LeavingMap;
	/** The match failed and cannot continue. */
	extern ENGINE_API const FName Aborted;
} // namespace MatchState

/**
 * A game mode with a match state machine (UE: AGameMode): EnteringMap → WaitingToStart (StartPlay) → InProgress
 * (StartMatch) → WaitingPostMatch (EndMatch), or Aborted (AbortMatch). Every change calls OnMatchStateSet, which runs
 * the matching Handle* and copies the state to an AGameState. Its game state class is AGameState.
 */
UCLASS()
class ENGINE_API AGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The current state (UE: GetMatchState). */
	[[nodiscard]] FName GetMatchState() const
	{
		return MatchState;
	}

	/** True while the match is in progress (UE: IsMatchInProgress). */
	[[nodiscard]] virtual bool IsMatchInProgress() const;
	/** True once the match has started, ended or not (UE: HasMatchStarted). */
	[[nodiscard]] virtual bool HasMatchStarted() const;
	/** True once the match has ended (UE: HasMatchEnded). */
	[[nodiscard]] virtual bool HasMatchEnded() const;

	/** WaitingToStart once the world plays (UE). */
	void StartPlay() override;
	/** InProgress (UE). */
	void StartMatch() override;
	/** WaitingPostMatch (UE). */
	void EndMatch() override;
	/** Aborted (UE). */
	virtual void AbortMatch();

protected:
	/** Changes the state and calls OnMatchStateSet (UE). */
	virtual void SetMatchState(FName NewState);
	/** Runs the Handle* of the new state and copies it to the AGameState (UE). */
	virtual void OnMatchStateSet();

	virtual void HandleMatchIsWaitingToStart();
	virtual void HandleMatchHasStarted();
	virtual void HandleMatchHasEnded();
	virtual void HandleLeavingMap();
	virtual void HandleMatchAborted();

	/** UE: MatchState. */
	UPROPERTY(Transient)
	FName MatchState;
};

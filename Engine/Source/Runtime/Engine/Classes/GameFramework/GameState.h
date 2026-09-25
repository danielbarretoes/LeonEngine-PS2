#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "GameState.generated.h"

/**
 * The game state of an AGameMode (UE: AGameState): the match state the mode sets, and the seconds elapsed while it is
 * in progress. Entering InProgress / WaitingPostMatch starts / ends the match clock of AGameStateBase.
 */
UCLASS()
class ENGINE_API AGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** UE: GetMatchState. */
	[[nodiscard]] FName GetMatchState() const
	{
		return MatchState;
	}
	/** The state before the last change (UE: PreviousMatchState). */
	[[nodiscard]] FName GetPreviousMatchState() const
	{
		return PreviousMatchState;
	}

	/** Begin play only (UE): the match clock starts when the match state is InProgress. */
	void HandleBeginPlay() override;

	/** Called by AGameMode (UE: SetMatchState): records the state and runs OnRep_MatchState. */
	virtual void SetMatchState(FName NewState);

	/** UE: IsMatchInProgress. */
	[[nodiscard]] bool IsMatchInProgress() const;

	/** Whole seconds the match has been in progress (UE: ElapsedTime). */
	UPROPERTY()
	int32 ElapsedTime = 0;

	void Tick(float DeltaTime) override;

protected:
	/** Reacts to a new state (UE: OnRep_MatchState; called directly, Leon does not replicate). */
	virtual void OnRep_MatchState();

	/** UE: MatchState. */
	UPROPERTY()
	FName MatchState;

	/** UE: PreviousMatchState. */
	UPROPERTY()
	FName PreviousMatchState;

private:
	/** Fraction of a second carried between ticks for ElapsedTime. */
	float ElapsedRemainder = 0.0f;
};

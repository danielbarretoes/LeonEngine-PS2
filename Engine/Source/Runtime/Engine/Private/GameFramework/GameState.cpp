#include "GameFramework/GameState.h"

#include "GameFramework/GameMode.h"

AGameState::AGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	MatchState = MatchState::EnteringMap;
	PreviousMatchState = MatchState::EnteringMap;
}

void AGameState::SetMatchState(FName NewState)
{
	if (MatchState == NewState)
	{
		return;
	}
	PreviousMatchState = MatchState;
	MatchState = NewState;
	OnRep_MatchState();
}

void AGameState::HandleBeginPlay()
{
	// Unlike the base, the match does not start with play: it waits for MatchState::InProgress (OnRep_MatchState).
	MarkHasBegunPlay();
}

bool AGameState::IsMatchInProgress() const
{
	return MatchState == MatchState::InProgress;
}

void AGameState::OnRep_MatchState()
{
	if (MatchState == MatchState::InProgress)
	{
		ElapsedTime = 0;
		ElapsedRemainder = 0.0f;
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::WaitingPostMatch)
	{
		HandleMatchHasEnded();
	}
}

void AGameState::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!IsMatchInProgress())
	{
		return;
	}
	// UE counts whole seconds with a one-second timer (DefaultTimer).
	ElapsedRemainder += DeltaTime;
	while (ElapsedRemainder >= 1.0f)
	{
		ElapsedRemainder -= 1.0f;
		++ElapsedTime;
	}
}

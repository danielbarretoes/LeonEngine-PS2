#include "GameFramework/GameState.h"

#include "Engine/World.h"
#include "GameFramework/GameMode.h"
#include "TimerManager.h"

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
	UWorld* World = GetWorld();
	if (MatchState == MatchState::InProgress)
	{
		ElapsedTime = 0;
		HandleMatchHasStarted();
		if (World != nullptr)
		{
			World->GetTimerManager().SetTimer(TimerHandle_DefaultTimer, this, &AGameState::DefaultTimer, 1.0f, true);
		}
	}
	else if (MatchState == MatchState::WaitingPostMatch)
	{
		HandleMatchHasEnded();
		if (World != nullptr)
		{
			World->GetTimerManager().ClearTimer(TimerHandle_DefaultTimer);
		}
	}
}

void AGameState::DefaultTimer()
{
	if (IsMatchInProgress())
	{
		++ElapsedTime;
	}
}

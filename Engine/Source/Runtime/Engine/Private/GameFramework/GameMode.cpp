#include "GameFramework/GameMode.h"

#include "GameFramework/GameState.h"

namespace MatchState
{
	const FName EnteringMap(TEXT("EnteringMap"));
	const FName WaitingToStart(TEXT("WaitingToStart"));
	const FName InProgress(TEXT("InProgress"));
	const FName WaitingPostMatch(TEXT("WaitingPostMatch"));
	const FName LeavingMap(TEXT("LeavingMap"));
	const FName Aborted(TEXT("Aborted"));
} // namespace MatchState

AGameMode::AGameMode(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GameStateClass = AGameState::StaticClass();
	MatchState = MatchState::EnteringMap;
}

bool AGameMode::IsMatchInProgress() const
{
	return MatchState == MatchState::InProgress;
}

bool AGameMode::HasMatchStarted() const
{
	return MatchState != MatchState::EnteringMap && MatchState != MatchState::WaitingToStart;
}

bool AGameMode::HasMatchEnded() const
{
	return MatchState == MatchState::WaitingPostMatch;
}

void AGameMode::StartPlay()
{
	Super::StartPlay();
	if (MatchState == MatchState::EnteringMap)
	{
		SetMatchState(MatchState::WaitingToStart);
	}
}

void AGameMode::StartMatch()
{
	if (HasMatchStarted())
	{
		return;
	}
	SetMatchState(MatchState::InProgress);
}

void AGameMode::EndMatch()
{
	if (!IsMatchInProgress())
	{
		return;
	}
	SetMatchState(MatchState::WaitingPostMatch);
}

void AGameMode::AbortMatch()
{
	SetMatchState(MatchState::Aborted);
}

void AGameMode::SetMatchState(FName NewState)
{
	if (MatchState == NewState)
	{
		return;
	}
	MatchState = NewState;
	OnMatchStateSet();
	if (AGameState* FullGameState = GetGameState<AGameState>())
	{
		FullGameState->SetMatchState(NewState);
	}
}

void AGameMode::OnMatchStateSet()
{
	if (MatchState == MatchState::WaitingToStart)
	{
		HandleMatchIsWaitingToStart();
	}
	else if (MatchState == MatchState::InProgress)
	{
		HandleMatchHasStarted();
	}
	else if (MatchState == MatchState::WaitingPostMatch)
	{
		HandleMatchHasEnded();
	}
	else if (MatchState == MatchState::LeavingMap)
	{
		HandleLeavingMap();
	}
	else if (MatchState == MatchState::Aborted)
	{
		HandleMatchAborted();
	}
}

void AGameMode::HandleMatchIsWaitingToStart()
{
}

void AGameMode::HandleMatchHasStarted()
{
}

void AGameMode::HandleMatchHasEnded()
{
}

void AGameMode::HandleLeavingMap()
{
}

void AGameMode::HandleMatchAborted()
{
}

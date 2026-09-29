#include "GameFramework/GameStateBase.h"

#include "Engine/World.h"

AGameStateBase::AGameStateBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

float AGameStateBase::GetServerWorldTimeSeconds() const
{
	if (bMatchHasEnded)
	{
		return FMath::Max(0.0f, MatchEndSeconds - MatchStartSeconds);
	}
	if (!bMatchInProgress)
	{
		return 0.0f;
	}
	const UWorld* World = GetWorld();
	return World != nullptr ? FMath::Max(0.0f, World->GetTimeSeconds() - MatchStartSeconds) : 0.0f;
}

void AGameStateBase::HandleMatchHasStarted()
{
	const UWorld* World = GetWorld();
	MatchStartSeconds = World != nullptr ? World->GetTimeSeconds() : 0.0f;
	MatchEndSeconds = MatchStartSeconds;
	bMatchInProgress = true;
	bMatchHasEnded = false;
}

void AGameStateBase::HandleMatchHasEnded()
{
	const UWorld* World = GetWorld();
	MatchEndSeconds = World != nullptr ? World->GetTimeSeconds() : MatchStartSeconds;
	bMatchInProgress = false;
	bMatchHasEnded = true;
}

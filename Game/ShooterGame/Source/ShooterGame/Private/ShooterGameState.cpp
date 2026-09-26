#include "ShooterGameState.h"

AShooterGameState::AShooterGameState(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

float AShooterGameState::GetPhaseTimeRemaining(float Now) const
{
	return PhaseEndTime > 0.0f ? FMath::Max(0.0f, PhaseEndTime - Now) : 0.0f;
}

int32 AShooterGameState::GetTeamScore(EShooterTeam Team) const
{
	switch (Team)
	{
		case EShooterTeam::CT:
			return ScoreCT;
		case EShooterTeam::T:
			return ScoreT;
		case EShooterTeam::None:
			break;
	}
	return 0;
}

void AShooterGameState::AddTeamScore(EShooterTeam Team)
{
	if (Team == EShooterTeam::CT)
	{
		++ScoreCT;
	}
	else if (Team == EShooterTeam::T)
	{
		++ScoreT;
	}
}

void AShooterGameState::AddKillFeedEntry(const FShooterKillFeedEntry& Entry)
{
	KillFeed.Add(Entry);
	while (KillFeed.Num() > MaxKillFeedEntries)
	{
		KillFeed.RemoveAt(0);
	}
}

void AShooterGameState::ResetMatch()
{
	ScoreCT = 0;
	ScoreT = 0;
	RoundNumber = 0;
	BombState = EShooterBombState::None;
	BombExplodeTime = 0.0f;
	BombSite = NAME_None;
	LastRoundEndReason = EShooterRoundEndReason::None;
	MatchWinner = EShooterTeam::None;
	KillFeed.Reset();
}

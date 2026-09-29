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

void AShooterGameState::BeginSecondHalf()
{
	// The players of each side move to the other: their rounds go with them.
	Swap(ScoreCT, ScoreT);
	bSecondHalf = true;
	HalftimeRound = RoundNumber;
	++HalftimeSerial;
}

void AShooterGameState::AddKillFeedEntry(const FShooterKillFeedEntry& Entry)
{
	KillFeed.Add(Entry);
	++KillFeedSerial;
	while (KillFeed.Num() > MaxKillFeedEntries)
	{
		KillFeed.RemoveAt(0);
	}
}

void AShooterGameState::AddRadioEntry(const FShooterRadioEntry& Entry)
{
	RadioLog.Add(Entry);
	++RadioSerial;
	while (RadioLog.Num() > MaxRadioEntries)
	{
		RadioLog.RemoveAt(0);
	}
}

bool AShooterGameState::WasRadioSentSince(EShooterTeam Team, EShooterRadioMessage Message, float Since) const
{
	for (const FShooterRadioEntry& Entry : RadioLog)
	{
		if (Entry.Team == Team && Entry.Message == Message && Entry.Time >= Since)
		{
			return true;
		}
	}
	return false;
}

void AShooterGameState::ResetMatch()
{
	ScoreCT = 0;
	ScoreT = 0;
	bSecondHalf = false;
	HalftimeRound = 0;
	RoundNumber = 0;
	++MatchSerial;
	BombState = EShooterBombState::None;
	BombExplodeTime = 0.0f;
	BombSite = NAME_None;
	LastRoundEndReason = EShooterRoundEndReason::None;
	MatchWinner = EShooterTeam::None;
	KillFeed.Reset();
	++KillFeedSerial;
	RadioLog.Reset();
	++RadioSerial;
}

#include "ShooterMatchChecker.h"

#include "AI/Navigation/NavigationSystem.h"
#include "Engine/World.h"
#include "ShooterCharacter.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterPlayerState.h"

void FShooterMatchChecker::AddViolation(const FString& Violation)
{
	if (!Violations.Contains(Violation))
	{
		Violations.Add(Violation);
	}
}

void FShooterMatchChecker::CheckRoundEnd(int32 RoundNumber, EShooterRoundEndReason Reason, int32 ScoreCT, int32 ScoreT)
{
	Reasons.Add(Reason);
	if (Reason == EShooterRoundEndReason::None)
	{
		AddViolation(FString::Printf(TEXT("round %d ended without a reason"), RoundNumber));
	}
	const EShooterTeam Winner = GetRoundEndWinner(Reason);
	const int32 ExpectedCT = LastScoreCT + (Winner == EShooterTeam::CT ? 1 : 0);
	const int32 ExpectedT = LastScoreT + (Winner == EShooterTeam::T ? 1 : 0);
	if (ScoreCT != ExpectedCT || ScoreT != ExpectedT)
	{
		AddViolation(FString::Printf(TEXT("round %d (%s): the score is CT %d - T %d, expected CT %d - T %d"),
			RoundNumber, GetRoundEndMessage(Reason), ScoreCT, ScoreT, ExpectedCT, ExpectedT));
	}
	DecidedRounds += Winner != EShooterTeam::None ? 1 : 0;
	if (ScoreCT + ScoreT != DecidedRounds)
	{
		AddViolation(
			FString::Printf(TEXT("round %d: the scores (CT %d - T %d) do not add up to the %d decided round(s)"),
				RoundNumber, ScoreCT, ScoreT, DecidedRounds));
	}
	LastScoreCT = ScoreCT;
	LastScoreT = ScoreT;
}

void FShooterMatchChecker::CheckHalftime(
	const AShooterGameMode& GameMode, int32 RoundNumber, int32 ScoreCT, int32 ScoreT)
{
	++NumHalftimes;
	// The swap comes between the halftime round's end and the next round's start (the new round is under way now).
	const int32 HalftimeRound = GameMode.GetHalftimeRound();
	if (HalftimeRound == 0 || RoundNumber != HalftimeRound + 1)
	{
		AddViolation(FString::Printf(
			TEXT("round %d: the teams switched sides, the halftime is after round %d"), RoundNumber, HalftimeRound));
	}
	// The scores follow the teams: each side has the other's rounds.
	if (ScoreCT != LastScoreT || ScoreT != LastScoreCT)
	{
		AddViolation(
			FString::Printf(TEXT("round %d: after the halftime the score is CT %d - T %d, expected CT %d - T %d"),
				RoundNumber, ScoreCT, ScoreT, LastScoreT, LastScoreCT));
	}
	LastScoreCT = ScoreCT;
	LastScoreT = ScoreT;
	for (const APlayerState* PlayerState : GameMode.GetGameState().GetPlayerArray())
	{
		const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(PlayerState);
		const int32 Index = RecordedStates.IndexOfByKey(ShooterState);
		if (ShooterState == nullptr || Index == INDEX_NONE)
		{
			continue; // joined since the last tick
		}
		const EShooterTeam Before = RecordedTeams[Index];
		if (Before != EShooterTeam::None && ShooterState->GetTeam() != GetOpposingTeam(Before))
		{
			AddViolation(FString::Printf(TEXT("round %d: %s stayed on %s at the halftime"), RoundNumber,
				*ShooterState->GetPlayerName(), GetShooterTeamName(ShooterState->GetTeam())));
		}
		// The money starts over (buying in the new freeze only lowers it).
		if (Before != EShooterTeam::None && ShooterState->GetMoney() > GameMode.StartMoney)
		{
			AddViolation(FString::Printf(TEXT("round %d: %s kept $%d over the halftime (start money $%d)"), RoundNumber,
				*ShooterState->GetPlayerName(), ShooterState->GetMoney(), GameMode.StartMoney));
		}
	}
}

void FShooterMatchChecker::RecordTeams(const AShooterGameMode& GameMode)
{
	RecordedStates.Reset();
	RecordedTeams.Reset();
	for (const APlayerState* PlayerState : GameMode.GetGameState().GetPlayerArray())
	{
		if (const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(PlayerState))
		{
			RecordedStates.Add(ShooterState);
			RecordedTeams.Add(ShooterState->GetTeam());
		}
	}
}

void FShooterMatchChecker::Tick(const AShooterGameMode& GameMode)
{
	const AShooterGameState* State = GameMode.GetShooterGameState();
	const UWorld* World = GameMode.GetWorld();
	if (State == nullptr || World == nullptr)
	{
		return;
	}
	const int32 RoundNumber = State->GetRoundNumber();
	if (State->GetMatchSerial() != LastMatchSerial)
	{
		// A new match (mp_restartgame, at any round): the score starts over, the rounds played are kept.
		LastMatchSerial = State->GetMatchSerial();
		LastScoreCT = 0;
		LastScoreT = 0;
		DecidedRounds = 0;
	}

	if (State->GetHalftimeSerial() != LastHalftimeSerial)
	{
		// The first tick only learns the serial (a checker may start in the second half).
		if (LastHalftimeSerial >= 0)
		{
			CheckHalftime(
				GameMode, RoundNumber, State->GetTeamScore(EShooterTeam::CT), State->GetTeamScore(EShooterTeam::T));
		}
		LastHalftimeSerial = State->GetHalftimeSerial();
	}
	const int32 HalftimeRound = GameMode.GetHalftimeRound();
	if (HalftimeRound > 0 && RoundNumber > HalftimeRound && !State->IsSecondHalf())
	{
		AddViolation(FString::Printf(
			TEXT("round %d: the teams did not switch sides after round %d"), RoundNumber, HalftimeRound));
	}

	const EShooterRoundState RoundState = State->GetRoundState();
	if ((RoundState == EShooterRoundState::RoundEnd || RoundState == EShooterRoundState::MatchEnd) &&
		State->GetRoundSerial() != LastEndedRoundSerial)
	{
		LastEndedRoundSerial = State->GetRoundSerial();
		CheckRoundEnd(RoundNumber, State->GetLastRoundEndReason(), State->GetTeamScore(EShooterTeam::CT),
			State->GetTeamScore(EShooterTeam::T));
	}
	RecordTeams(GameMode);

	for (const APlayerState* PlayerState : GameMode.GetGameState().GetPlayerArray())
	{
		const AShooterPlayerState* ShooterState = Cast<AShooterPlayerState>(PlayerState);
		if (ShooterState != nullptr && (ShooterState->GetMoney() < 0 || ShooterState->GetMoney() > GameMode.MaxMoney))
		{
			AddViolation(FString::Printf(TEXT("round %d: %s has $%d, outside [0, %d]"), RoundNumber,
				*ShooterState->GetPlayerName(), ShooterState->GetMoney(), GameMode.MaxMoney));
		}
	}
	for (const EShooterTeam Team : {EShooterTeam::CT, EShooterTeam::T})
	{
		if (GameMode.GetTeamSize(Team) > GameMode.MaxPlayersPerTeam)
		{
			AddViolation(FString::Printf(TEXT("round %d: %s has %d players, more than %d"), RoundNumber,
				GetShooterTeamName(Team), GameMode.GetTeamSize(Team), GameMode.MaxPlayersPerTeam));
		}
	}

	const TArray<UNavigationSystem::FNode>& Nodes = World->GetNavigationSystem().GetNodes();
	float LowestFloor = TNumericLimits<float>::Max();
	for (const UNavigationSystem::FNode& Node : Nodes)
	{
		LowestFloor = FMath::Min(LowestFloor, Node.Location.Z);
	}
	// The game mode's pawns (its registry), not the level's actors: this runs every frame of a bot match.
	for (const AShooterCharacter* Pawn : GameMode.GetPawns())
	{
		if (Pawn->IsPendingKillPending() || !Pawn->IsAlive())
		{
			continue;
		}
		if (Pawn->GetHealth() <= 0.0f || Pawn->GetHealth() > Pawn->GetMaxHealth())
		{
			AddViolation(FString::Printf(TEXT("round %d: %s is alive with %.0f health"), RoundNumber, *Pawn->GetName(),
				static_cast<double>(Pawn->GetHealth())));
		}
		// Leon's character stands on its location (ACharacter: the actor location is the capsule's feet).
		const float Feet = Pawn->GetActorLocation().Z;
		if (Nodes.Num() > 0 && Feet < LowestFloor - FloorTolerance)
		{
			AddViolation(FString::Printf(TEXT("round %d: %s fell through the floor (feet at %.0f, lowest floor %.0f)"),
				RoundNumber, *Pawn->GetName(), static_cast<double>(Feet), static_cast<double>(LowestFloor)));
		}
	}
}

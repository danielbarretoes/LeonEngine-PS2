#pragma once

#include "CoreMinimal.h"
#include "ShooterTypes.h"

class AShooterGameMode;
class AShooterPlayerState;

/**
 * Watches a match and checks the invariants a round keeps (plan P21: the headless bot match, `-botmatch`, and the
 * bots' tests run it every frame). Not a UObject: its owner calls Tick once a frame.
 *
 * - Each round that ends has a reason, and its winner's score (the reason's, GetRoundEndWinner) went up by one and
 *   the loser's did not; the scores add up to the rounds with a winner.
 * - Every player's money stays within [0, MaxMoney]; no team has more than MaxPlayersPerTeam players.
 * - A live pawn has health within (0, its max] and its feet no lower than the navigation's lowest floor less
 *   FloorTolerance (nobody falls through the map).
 * - A new match (mp_restartgame; AShooterGameState's match serial) starts the score over.
 * - The halftime (AShooterGameState's halftime serial) comes after the game mode's halftime round, and there: every
 *   player of a team is on the other one, the scores swapped sides with them, and nobody has more than StartMoney.
 *   A round played past the halftime round without it is a violation too (the swap must happen).
 *
 * A broken invariant is kept as a line (GetViolations), once per kind and round.
 */
class SHOOTERGAME_API FShooterMatchChecker
{
public:
	/** How far below the lowest waypoint floor a pawn's feet may be, cm (the floor probes and the steps' slack). */
	static constexpr float FloorTolerance = 60.0f;

	/** Observes the game mode's match now: records a round that ended, checks the invariants. */
	void Tick(const AShooterGameMode& GameMode);

	/** The rounds that ended since the checker started (or the last new match). */
	[[nodiscard]] int32 GetRoundsPlayed() const
	{
		return Reasons.Num();
	}
	/** Why each of them ended, in order. */
	[[nodiscard]] const TArray<EShooterRoundEndReason>& GetRoundEndReasons() const
	{
		return Reasons;
	}
	[[nodiscard]] const TArray<FString>& GetViolations() const
	{
		return Violations;
	}
	[[nodiscard]] bool HasViolations() const
	{
		return Violations.Num() > 0;
	}
	/** How many halftimes the checker saw (each checked). */
	[[nodiscard]] int32 GetNumHalftimes() const
	{
		return NumHalftimes;
	}

private:
	void AddViolation(const FString& Violation);
	void CheckRoundEnd(int32 RoundNumber, EShooterRoundEndReason Reason, int32 ScoreCT, int32 ScoreT);
	/** The teams just switched sides: checks the swap against the teams and scores seen before it. */
	void CheckHalftime(const AShooterGameMode& GameMode, int32 RoundNumber, int32 ScoreCT, int32 ScoreT);
	/** Keeps every player's team (the next halftime is checked against them); no allocation once grown. */
	void RecordTeams(const AShooterGameMode& GameMode);

	TArray<EShooterRoundEndReason> Reasons;
	TArray<FString> Violations;
	/** The round serial whose end was recorded last, and the match serial observed (AShooterGameState). */
	int32 LastEndedRoundSerial = -1;
	int32 LastMatchSerial = -1;
	int32 LastScoreCT = 0;
	int32 LastScoreT = 0;
	int32 DecidedRounds = 0;
	/** The halftime serial observed (-1: none yet), how many were seen, and the players' teams of the last tick. */
	int32 LastHalftimeSerial = -1;
	int32 NumHalftimes = 0;
	TArray<const AShooterPlayerState*> RecordedStates;
	TArray<EShooterTeam> RecordedTeams;
};

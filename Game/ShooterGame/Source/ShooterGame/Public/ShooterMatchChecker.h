#pragma once

#include "CoreMinimal.h"
#include "ShooterTypes.h"

class AShooterGameMode;

/**
 * Watches a match and checks the invariants a round keeps (plan P21: the headless bot match, `-botmatch`, and the
 * bots' tests run it every frame). Not a UObject: its owner calls Tick once a frame.
 *
 * - Each round that ends has a reason, and its winner's score (the reason's, GetRoundEndWinner) went up by one and
 *   the loser's did not; the scores add up to the rounds with a winner.
 * - Every player's money stays within [0, MaxMoney]; no team has more than MaxPlayersPerTeam players.
 * - A live pawn has health within (0, its max] and its feet no lower than the navigation's lowest floor less
 *   FloorTolerance (nobody falls through the map).
 * - The round number never goes back, except when a new match starts (mp_restartgame) and the checker starts over.
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

private:
	void AddViolation(const FString& Violation);
	void CheckRoundEnd(int32 RoundNumber, EShooterRoundEndReason Reason, int32 ScoreCT, int32 ScoreT);

	TArray<EShooterRoundEndReason> Reasons;
	TArray<FString> Violations;
	/** The round whose end was recorded last (0: none). */
	int32 LastEndedRound = 0;
	int32 LastRoundNumber = 0;
	int32 LastScoreCT = 0;
	int32 LastScoreT = 0;
	int32 DecidedRounds = 0;
};

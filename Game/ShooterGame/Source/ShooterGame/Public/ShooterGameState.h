#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "ShooterTypes.h"
#include "ShooterGameState.generated.h"

/** One line of the kill feed (CS: "Killer [weapon] Victim", a headshot marked). */
struct FShooterKillFeedEntry
{
	FString KillerName;
	FString VictimName;
	FString WeaponName;
	EShooterTeam KillerTeam = EShooterTeam::None;
	EShooterTeam VictimTeam = EShooterTeam::None;
	bool bHeadshot = false;
	/** The world time of the kill. */
	float Time = 0.0f;
};

/**
 * What everyone sees of a ShooterGame match (UE ShooterGame: AShooterGameState): the round's phase and number, when the
 * phase ends, the teams' scores, the bomb's state and the kill feed. AShooterGameMode writes it; the HUD reads it.
 */
UCLASS()
class SHOOTERGAME_API AShooterGameState : public AGameState
{
	GENERATED_BODY()

public:
	AShooterGameState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** How many kill feed lines are kept (the newest last). */
	static constexpr int32 MaxKillFeedEntries = 5;

	[[nodiscard]] EShooterRoundState GetRoundState() const
	{
		return RoundState;
	}
	/** The round being played, from 1 (0 in the warmup). */
	[[nodiscard]] int32 GetRoundNumber() const
	{
		return RoundNumber;
	}
	/** The world time the current phase ends (the freeze, the round's time, the result's display); 0 without one. */
	[[nodiscard]] float GetPhaseEndTime() const
	{
		return PhaseEndTime;
	}
	/** Seconds left in the phase at world time Now (0 without a timed phase). */
	[[nodiscard]] float GetPhaseTimeRemaining(float Now) const;
	/** The world time the buy time ends. */
	[[nodiscard]] float GetBuyEndTime() const
	{
		return BuyEndTime;
	}
	/** Nobody moves or shoots (the freeze time). */
	[[nodiscard]] bool IsFreezeTime() const
	{
		return RoundState == EShooterRoundState::Freeze;
	}

	/** Rounds a team has won. */
	[[nodiscard]] int32 GetTeamScore(EShooterTeam Team) const;

	[[nodiscard]] EShooterBombState GetBombState() const
	{
		return BombState;
	}
	/** The planted bomb's explosion time (0 unless planted). */
	[[nodiscard]] float GetBombExplodeTime() const
	{
		return BombExplodeTime;
	}
	/** The site the bomb was planted in ("A", "B"), NAME_None otherwise. */
	[[nodiscard]] FName GetBombSite() const
	{
		return BombSite;
	}

	/** The last round's end (None before the first). */
	[[nodiscard]] EShooterRoundEndReason GetLastRoundEndReason() const
	{
		return LastRoundEndReason;
	}
	/** The team that won the match (None until it ends, or on a tie). */
	[[nodiscard]] EShooterTeam GetMatchWinner() const
	{
		return MatchWinner;
	}

	[[nodiscard]] const TArray<FShooterKillFeedEntry>& GetKillFeed() const
	{
		return KillFeed;
	}
	void AddKillFeedEntry(const FShooterKillFeedEntry& Entry);

	// Written by AShooterGameMode

	void SetRoundState(EShooterRoundState NewState, float NewPhaseEndTime)
	{
		RoundState = NewState;
		PhaseEndTime = NewPhaseEndTime;
	}
	void SetRoundNumber(int32 NewRoundNumber)
	{
		RoundNumber = NewRoundNumber;
	}
	void SetBuyEndTime(float Time)
	{
		BuyEndTime = Time;
	}
	void SetBombState(EShooterBombState NewState, float ExplodeTime = 0.0f, FName Site = NAME_None)
	{
		BombState = NewState;
		BombExplodeTime = ExplodeTime;
		BombSite = Site;
	}
	void AddTeamScore(EShooterTeam Team);
	void SetLastRoundEndReason(EShooterRoundEndReason Reason)
	{
		LastRoundEndReason = Reason;
	}
	void SetMatchWinner(EShooterTeam Team)
	{
		MatchWinner = Team;
	}
	/** Back to a new match: scores, round number, the feed. */
	void ResetMatch();

private:
	UPROPERTY()
	EShooterRoundState RoundState = EShooterRoundState::Warmup;

	UPROPERTY()
	int32 RoundNumber = 0;

	UPROPERTY()
	float PhaseEndTime = 0.0f;

	UPROPERTY()
	float BuyEndTime = 0.0f;

	UPROPERTY()
	int32 ScoreCT = 0;

	UPROPERTY()
	int32 ScoreT = 0;

	UPROPERTY()
	EShooterBombState BombState = EShooterBombState::None;

	UPROPERTY()
	float BombExplodeTime = 0.0f;

	UPROPERTY()
	FName BombSite;

	UPROPERTY()
	EShooterRoundEndReason LastRoundEndReason = EShooterRoundEndReason::None;

	UPROPERTY()
	EShooterTeam MatchWinner = EShooterTeam::None;

	TArray<FShooterKillFeedEntry> KillFeed;
};

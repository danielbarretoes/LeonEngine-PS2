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

/** One radio message (CS 1.6's radio, AShooterGameMode::SendRadioMessage): its sender, the team that hears it, what. */
struct FShooterRadioEntry
{
	FString SenderName;
	EShooterTeam Team = EShooterTeam::None;
	EShooterRadioMessage Message = EShooterRadioMessage::None;
	/** Where it is about (the enemy spotted; else where the sender stood). */
	FVector Location = FVector::ZeroVector;
	/** The world time it was sent. */
	float Time = 0.0f;
};

/**
 * What everyone sees of a ShooterGame match (UE ShooterGame: AShooterGameState): the round's phase and number, when the
 * phase ends, the teams' scores, the bomb's state, the kill feed and the radio's last messages. AShooterGameMode writes
 * it; the HUD reads it.
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
	/**
	 * Counts every round started and every match begun, never going back (mp_restartgame starts the round number
	 * over, not these): a new value tells an observer (a bot, the match checker) that a new round or match began.
	 */
	[[nodiscard]] int32 GetRoundSerial() const
	{
		return RoundSerial;
	}
	[[nodiscard]] int32 GetMatchSerial() const
	{
		return MatchSerial;
	}
	/** The world time the current phase ends (the freeze, the round's time, the result's display); 0 without one. */
	[[nodiscard]] float GetPhaseEndTime() const
	{
		return PhaseEndTime;
	}
	/** Seconds left in the phase at world time Now (0 without a timed phase). */
	[[nodiscard]] float GetPhaseTimeRemaining(float Now) const;
	/** The world time the buy time ends: BuyTime after the freeze's end (CS's mp_buytime). */
	[[nodiscard]] float GetBuyEndTime() const
	{
		return BuyEndTime;
	}
	/** Nobody moves or shoots (the freeze time). */
	[[nodiscard]] bool IsFreezeTime() const
	{
		return RoundState == EShooterRoundState::Freeze;
	}

	/** Rounds a team has won (the rounds of the players now on that side: the scores follow the teams at halftime). */
	[[nodiscard]] int32 GetTeamScore(EShooterTeam Team) const;

	/** The teams switched sides at halftime (AShooterGameMode::HandleHalftime) and play the second half. */
	[[nodiscard]] bool IsSecondHalf() const
	{
		return bSecondHalf;
	}
	/** The round after which the teams switched sides (0 before the halftime). */
	[[nodiscard]] int32 GetHalftimeRound() const
	{
		return bSecondHalf ? HalftimeRound : 0;
	}
	/** Counts every halftime, never going back (a new match does not): a new value tells an observer the sides swapped.
	 */
	[[nodiscard]] int32 GetHalftimeSerial() const
	{
		return HalftimeSerial;
	}

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
	/** Changes whenever the kill feed does (an entry added, the match reset): the HUD formats the feed again. */
	[[nodiscard]] int32 GetKillFeedSerial() const
	{
		return KillFeedSerial;
	}
	void AddKillFeedEntry(const FShooterKillFeedEntry& Entry);

	/** How many radio messages are kept (the newest last), both teams'. */
	static constexpr int32 MaxRadioEntries = 6;

	/** The last radio messages, both teams' (the HUD shows its team's). */
	[[nodiscard]] const TArray<FShooterRadioEntry>& GetRadioLog() const
	{
		return RadioLog;
	}
	/** Changes whenever the radio log does: the HUD formats its lines again. */
	[[nodiscard]] int32 GetRadioSerial() const
	{
		return RadioSerial;
	}
	void AddRadioEntry(const FShooterRadioEntry& Entry);
	/** Team heard Message at or after world time Since (the bots do not repeat what a teammate just said). */
	[[nodiscard]] bool WasRadioSentSince(EShooterTeam Team, EShooterRadioMessage Message, float Since) const;

	// Written by AShooterGameMode

	void SetRoundState(EShooterRoundState NewState, float NewPhaseEndTime)
	{
		RoundState = NewState;
		PhaseEndTime = NewPhaseEndTime;
	}
	/** A round starts with this number (and a new round serial). */
	void SetRoundNumber(int32 NewRoundNumber)
	{
		RoundNumber = NewRoundNumber;
		++RoundSerial;
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
	/**
	 * The halftime after the current round: the scores swap sides with the teams, the second half begins (and a new
	 * halftime serial).
	 */
	void BeginSecondHalf();
	void SetLastRoundEndReason(EShooterRoundEndReason Reason)
	{
		LastRoundEndReason = Reason;
	}
	void SetMatchWinner(EShooterTeam Team)
	{
		MatchWinner = Team;
	}
	/** Back to a new match: scores, round number, the first half, the feed (and a new match serial). */
	void ResetMatch();

private:
	UPROPERTY()
	EShooterRoundState RoundState = EShooterRoundState::Warmup;

	UPROPERTY()
	int32 RoundNumber = 0;

	UPROPERTY()
	int32 RoundSerial = 0;

	UPROPERTY()
	int32 MatchSerial = 0;

	UPROPERTY()
	int32 KillFeedSerial = 0;

	UPROPERTY()
	float PhaseEndTime = 0.0f;

	UPROPERTY()
	float BuyEndTime = 0.0f;

	UPROPERTY()
	int32 ScoreCT = 0;

	UPROPERTY()
	int32 ScoreT = 0;

	UPROPERTY()
	bool bSecondHalf = false;

	UPROPERTY()
	int32 HalftimeRound = 0;

	UPROPERTY()
	int32 HalftimeSerial = 0;

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

	TArray<FShooterRadioEntry> RadioLog;
	int32 RadioSerial = 0;
};

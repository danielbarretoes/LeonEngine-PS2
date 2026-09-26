#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "ShooterTypes.h"
#include "ShooterPlayerState.generated.h"

/**
 * A player's state in a ShooterGame match (UE ShooterGame: AShooterPlayerState): the team the game mode placed it in,
 * whether it is a bot, its money (Counter-Strike's, up to the game mode's MaxMoney), and its kills and deaths.
 */
UCLASS()
class SHOOTERGAME_API AShooterPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AShooterPlayerState(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The team (UE ShooterGame: SetTeamNum / GetTeamNum). */
	void SetTeam(EShooterTeam NewTeam)
	{
		Team = NewTeam;
	}
	[[nodiscard]] EShooterTeam GetTeam() const
	{
		return Team;
	}

	/** The player is a bot (UE: APlayerState::bIsABot). */
	UPROPERTY()
	bool bIsABot = false;

	[[nodiscard]] int32 GetMoney() const
	{
		return Money;
	}
	/** Sets the money, clamped to [0, MaxMoney]. */
	void SetMoney(int32 NewMoney, int32 MaxMoney);
	/** Adds (or takes, negative) money, clamped to [0, MaxMoney]; returns the change made. */
	int32 AddMoney(int32 Amount, int32 MaxMoney);

	/** UE ShooterGame: GetKills / GetDeaths (ScoreKill / ScoreDeath add to them). */
	[[nodiscard]] int32 GetKills() const
	{
		return NumKills;
	}
	[[nodiscard]] int32 GetDeaths() const
	{
		return NumDeaths;
	}
	/** A kill (a team kill takes one away, CS's frags) and the score. */
	void ScoreKill(int32 Points);
	void ScoreDeath();
	/** Back to a new match: no kills, no deaths (the money is the game mode's). */
	void ResetStats();

private:
	/** UE ShooterGame: TeamNumber. */
	UPROPERTY()
	EShooterTeam Team = EShooterTeam::None;

	UPROPERTY()
	int32 Money = 0;

	/** UE ShooterGame: NumKills, NumDeaths. */
	UPROPERTY()
	int32 NumKills = 0;

	UPROPERTY()
	int32 NumDeaths = 0;
};

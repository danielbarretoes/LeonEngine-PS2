#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "ShooterTypes.h"
#include "ShooterGameMode.generated.h"

class AShooterAIController;
class AShooterPlayerState;

/**
 * ShooterGame's rules (UE ShooterGame: AShooterGameMode), the game mode of every map through GlobalDefaultGameMode
 * (plan decision D18). P17: teams and team spawns.
 *
 * - A joining player takes a team (ChooseTeam): `?team=CT` / `?team=T` in the map URL, else the smaller team (CT on a
 *   tie). A bot joins the team it is added to (bot_add_ct / bot_add_t, AddBots).
 * - ChoosePlayerStart picks, in level order, the first free APlayerStart whose PlayerStartTag is the team's tag ("CT",
 *   "T"; the map's PlayerStart_CT / PlayerStart_T nodes). A start is free when no pawn stands within two capsule radii
 *   of it. With every start taken it reuses the team's first; a map without team starts uses the engine's choice.
 * - The pawn stands on the start: a start's location is its capsule's centre (UE), Leon's character stands on its feet,
 *   so SpawnDefaultPawnFor lowers it by the start capsule's half height.
 *
 * Console (the Exec chain reaches the game mode): `bot_add_ct [N]`, `bot_add_t [N]`, `bot_add [N]` (the smaller team)
 * add bots, CS's commands (plan P19), and `bot_fill` fills both teams to MaxPlayersPerTeam (the G6 smoke:
 * `ShooterGame -nullrhi -ExecCmds=bot_fill`); the bots have no brain until P20.
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AShooterGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The players a team takes (CS: 5 a side); more bots are refused. */
	UPROPERTY(Config)
	int32 MaxPlayersPerTeam = 5;

	/** The class of the bots' controllers. */
	UPROPERTY()
	TSubclassOf<AShooterAIController> BotControllerClass;

	/** The team's first free start in level order (see the class comment). */
	[[nodiscard]] AActor* ChoosePlayerStart(AController* Player) override;
	/** Spawns the pawn standing on the start (see the class comment). */
	APawn* SpawnDefaultPawnFor(AController* NewPlayer, AActor* StartSpot) override;
	/** bot_add_ct / bot_add_t / bot_add [Count], bot_fill; the rest goes to the reflected Exec functions. */
	bool ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor) override;
	/** Logs how many pawns each team has when the match leaves (the G6 smoke reads it). */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Adds up to Count bots to Team (None: the smaller team each time), each at its team's free start; returns how
	 * many joined (a full team refuses the rest).
	 */
	int32 AddBots(EShooterTeam Team, int32 Count);

	/** Adds bots until both teams have MaxPlayersPerTeam (CS: bot_quota; plan P19's 5v5 fill); how many joined. */
	int32 FillTeamsWithBots();

	/** The team a new player joins: `?team=` of Options, else the smaller team, CT on a tie. */
	[[nodiscard]] EShooterTeam ChooseTeam(const FString& Options) const;

	/** How many players (and bots) a team has. */
	[[nodiscard]] int32 GetTeamSize(EShooterTeam Team) const;

	/** How many live pawns each team has. */
	void CountPawns(int32& OutCT, int32& OutT) const;

protected:
	/** Places the player in a team before its start is chosen (UE: InitNewPlayer). */
	FString InitNewPlayer(
		APlayerController* NewPlayerController, const FString& Options, const FString& Portal = FString()) override;
	/** Logs where a player spawned. */
	void RestartPlayer(AController* NewPlayer) override;

private:
	/** The number of the next bot of each team (their names: Bot_CT_1, Bot_T_1, ...). */
	int32 NextBotNumber[3] = {1, 1, 1};
};

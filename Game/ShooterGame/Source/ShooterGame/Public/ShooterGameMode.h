#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameMode.h"
#include "Math/RandomStream.h"
#include "ShooterMatchChecker.h"
#include "ShooterTypes.h"
#include "ShooterGameMode.generated.h"

class AShooterAIController;
class AShooterBomb;
class AShooterCharacter;
class AShooterGameState;
class AShooterPlayerState;
class ATriggerVolume;

/**
 * ShooterGame's rules (UE ShooterGame: AShooterGameMode), the game mode of every map through GlobalDefaultGameMode
 * (plan decision D18): Counter-Strike's defusal rules, teams, rounds, money, buying and the bomb.
 *
 * Teams and spawns (P17):
 * - A joining player takes a team (ChooseTeam): `?team=CT` / `?team=T` in the map URL, else the smaller team (CT on a
 *   tie). A bot joins the team it is added to (bot_add_ct / bot_add_t, AddBots).
 * - ChoosePlayerStart picks, in level order, the first free APlayerStart whose PlayerStartTag is the team's tag ("CT",
 *   "T"). A start is free when no pawn stands within two capsule radii of it. With every start taken it reuses the
 *   team's first; a map without team starts uses the engine's choice. A round's start places each team's players on
 *   its starts in the order they joined.
 * - The pawn stands on the start: a start's location is its capsule's centre (UE), Leon's character stands on its feet,
 *   so SpawnDefaultPawnFor lowers it by the start capsule's half height.
 *
 * Rounds (the match states of AGameMode, the round's phase in AShooterGameState):
 * - Warmup (WaitingToStart): players join and spawn at once. With bFillTeamsWithBots, bots fill both teams to
 *   MaxPlayersPerTeam as soon as a human is in. The match starts (StartMatch) when both teams have a player.
 * - Each round: Freeze (FreezeTime: pawns hold still, buying), Live (RoundTime), RoundEnd (RoundRestartDelay: the
 *   result shows), then the next round or, once a team has won more than half of MaxRounds or MaxRounds were played,
 *   MatchEnd (WaitingPostMatch). A round's start cleans the map (weapons on the floor, grenades, corpses, the last
 *   bomb), gives the survivors their health back with their weapons and armor, respawns the dead with the default
 *   inventory, and gives a new bomb to a random terrorist (RandomSeed's stream, or `?seed=` in the URL). A player who
 *   joins during Live or RoundEnd waits for the next round.
 * - The round ends (EndRound) when every terrorist is dead with no bomb planted, every counter-terrorist is dead, both
 *   teams die at once (a draw), the time runs out with no bomb planted (the CT win), or the planted bomb explodes (T)
 *   or is defused (CT). Planting stops the round's clock: the bomb's timer decides.
 *
 * Money (Counter-Strike 1.6's, all config): StartMoney at the match's start, at most MaxMoney; a kill pays the
 * weapon's KillReward (a team kill costs TeamKillPenalty); the winners get WinReward (BombWinReward for a bomb or a
 * defuse), the losers the loss bonus, LossBonusBase growing by LossBonusIncrement a consecutive loss up to
 * LossBonusMax, and the terrorists LosingTeamPlantBonus more when they lose with the bomb planted; the planter and the
 * defuser get PlantReward / DefuseReward.
 *
 * Buying (Buy): alive, in the team's buy zone (the map's `BuyZone` volumes tagged with the team), within BuyTime of the
 * round's start (any time in the warmup), with the money: a weapon by name (usp, ak47, awp, hegrenade: its Price;
 * the same weapon twice is refused, another one in the slot is dropped), `vest` (kevlar), `vesthelm` (kevlar and
 * helmet; the helmet alone with full kevlar) and `defuser` (CT only).
 *
 * Damage: CanDealDamage refuses a teammate's (bFriendlyFire false, CS's mp_friendlyfire 0); a player may hurt itself
 * (its own grenade). Killed hears of each death from AShooterCharacter::Die: the kill feed, the money, the kills and
 * deaths. Whether the round is over is checked on the next tick, so the deaths of one moment (an explosion that kills
 * the last of both teams) end it together (a draw).
 *
 * Console (the Exec chain reaches the game mode): `bot_add_ct [N]`, `bot_add_t [N]`, `bot_add [N]` (the smaller team),
 * `bot_fill` (both teams to MaxPlayersPerTeam; the G6 smoke: `ShooterGame -nullrhi -ExecCmds=bot_fill`),
 * `bot_kick [name|all]`, `bot_stop [0|1]` (the bots freeze), `mp_restartgame [seconds]` (a new match after that
 * many seconds, 1 by default).
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterGameMode : public AGameMode
{
	GENERATED_BODY()

public:
	AShooterGameMode(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The tags of the map's zones (plan decision D15: the second tag names the site or the team). */
	static const FName BombSiteTag;
	static const FName BuyZoneTag;

	/** The players a team takes (CS: 5 a side); more bots are refused. */
	UPROPERTY(Config)
	int32 MaxPlayersPerTeam = 5;

	/** Teammates hurt each other (CS: mp_friendlyfire). */
	UPROPERTY(Config)
	bool bFriendlyFire = false;

	/** The bots stand still and do nothing (CS: bot_stop 1; the `bot_stop [0|1]` command). */
	UPROPERTY(Config)
	bool bBotStop = false;

	/** Bots fill both teams as soon as a human player is in (CS: bot_quota with bot_quota_mode fill). */
	UPROPERTY(Config)
	bool bFillTeamsWithBots = false;

	/** Seconds of each phase (CS: mp_freezetime, mp_roundtime, the round restart delay) and of buying (mp_buytime). */
	UPROPERTY(Config)
	float FreezeTime = 6.0f;

	UPROPERTY(Config)
	float RoundTime = 115.0f;

	UPROPERTY(Config)
	float RoundRestartDelay = 5.0f;

	UPROPERTY(Config)
	float BuyTime = 45.0f;

	/** Rounds in a match (CS: mp_maxrounds); a team wins at more than half. */
	UPROPERTY(Config)
	int32 MaxRounds = 30;

	/** Money (CS 1.6; see the class comment). */
	UPROPERTY(Config)
	int32 StartMoney = 800;

	UPROPERTY(Config)
	int32 MaxMoney = 16000;

	UPROPERTY(Config)
	int32 WinReward = 3250;

	UPROPERTY(Config)
	int32 BombWinReward = 3500;

	UPROPERTY(Config)
	int32 LossBonusBase = 1400;

	UPROPERTY(Config)
	int32 LossBonusIncrement = 500;

	UPROPERTY(Config)
	int32 LossBonusMax = 3400;

	UPROPERTY(Config)
	int32 LosingTeamPlantBonus = 800;

	UPROPERTY(Config)
	int32 PlantReward = 300;

	UPROPERTY(Config)
	int32 DefuseReward = 300;

	UPROPERTY(Config)
	int32 TeamKillPenalty = 3300;

	/** Equipment prices (CS 1.6). */
	UPROPERTY(Config)
	int32 VestPrice = 650;

	UPROPERTY(Config)
	int32 VestHelmetPrice = 1000;

	UPROPERTY(Config)
	int32 HelmetPrice = 350;

	UPROPERTY(Config)
	int32 DefuserPrice = 200;

	/** The seed of the round stream (the bomb's carrier); `?seed=N` in the URL or `-seed=N` overrides it. */
	UPROPERTY(Config)
	int32 RandomSeed = 1;

	/** The class of the bots' controllers. */
	UPROPERTY()
	TSubclassOf<AShooterAIController> BotControllerClass;

	/** The class of the bomb (AShooterBomb). */
	UPROPERTY()
	TSubclassOf<AShooterBomb> BombClass;

	/**
	 * A headless bot match (plan P21; `-botmatch [-rounds=N] [-seed=N]` on the command line): the local player
	 * spectates, bots fill both teams, FShooterMatchChecker checks every frame, and after BotMatchRounds rounds (or
	 * the match's end, or a deadline for them) the game exits with 0, or 1 when an invariant broke. Run it with
	 * -nullrhi -benchmark to play faster than real time.
	 */
	bool bBotMatch = false;
	int32 BotMatchRounds = 10;

	void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	void Tick(float DeltaSeconds) override;
	/** The team's first free start in level order (see the class comment). */
	[[nodiscard]] AActor* ChoosePlayerStart(AController* Player) override;
	/** Spawns the pawn standing on the start (see the class comment). */
	APawn* SpawnDefaultPawnFor(AController* NewPlayer, AActor* StartSpot) override;
	/** bot_add_ct / bot_add_t / bot_add [Count], bot_fill, bot_kick, mp_restartgame; the rest goes to Exec functions.
	 */
	bool ProcessConsoleExec(const TCHAR* Cmd, FOutputDevice& Ar, UObject* Executor) override;
	/** Logs how many pawns each team has when the match leaves (the G6 smoke reads it). */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Adds up to Count bots to Team (None: the smaller team each time); returns how many joined (a full team refuses
	 * the rest). A bot spawns at once unless the round is live (it waits for the next one).
	 */
	int32 AddBots(EShooterTeam Team, int32 Count);

	/** Adds bots until both teams have MaxPlayersPerTeam (CS: bot_quota; plan P19's 5v5 fill); how many joined. */
	int32 FillTeamsWithBots();

	/** Removes a bot by player name ("all": every bot); how many left. */
	int32 KickBots(const FString& Name);

	/** The team a new player joins: `?team=` of Options, else the smaller team, CT on a tie. */
	[[nodiscard]] EShooterTeam ChooseTeam(const FString& Options) const;

	/** How many players (and bots) a team has. */
	[[nodiscard]] int32 GetTeamSize(EShooterTeam Team) const;

	/**
	 * Whether DamageInstigator's damage reaches Victim (UE ShooterGame: CanDealDamage): always without both players,
	 * from oneself, or across teams; within a team only with bFriendlyFire.
	 */
	[[nodiscard]] virtual bool CanDealDamage(AController* DamageInstigator, AController* Victim) const;

	/**
	 * A pawn died (UE ShooterGame: Killed): Killer is credited (the victim itself for a suicide or the world), Causer
	 * is what hurt it (a weapon or a projectile, null for a suicide).
	 */
	virtual void Killed(
		AController* Killer, AController* Victim, APawn* VictimPawn, AActor* DamageCauser, bool bHeadshot);

	/** How many kills Killed has heard of. */
	[[nodiscard]] int32 GetNumKills() const
	{
		return NumKills;
	}

	/** How many live pawns the players of each team have. */
	void CountPawns(int32& OutCT, int32& OutT) const;

	// Rounds

	/** The game state, as the game's class. */
	[[nodiscard]] AShooterGameState* GetShooterGameState() const;
	/** Both teams have a player (UE: ReadyToStartMatch). */
	[[nodiscard]] virtual bool ReadyToStartMatch() const;
	/** Cleans the map, places and respawns the players, gives out the bomb and starts the freeze. */
	void StartRound();
	/** The round is over: the score, the money, the message; the next round after RoundRestartDelay. */
	void EndRound(EShooterRoundEndReason Reason);
	/** Checks the eliminations and the time (each tick of a live round). */
	void CheckRoundEnd();
	/** A new match from the first round after Delay seconds (mp_restartgame). */
	void RestartGame(float Delay);
	/** The round's bomb, or null. */
	[[nodiscard]] AShooterBomb* GetBomb() const
	{
		return Bomb;
	}
	/**
	 * The bomb site the terrorists go for this round ("A" or "B"), drawn from the round stream at the round's start
	 * (the bots' plan); NAME_None without sites.
	 */
	[[nodiscard]] FName GetTerroristTargetSite() const
	{
		return TerroristTargetSite;
	}
	/** The map's bomb sites' names, sorted ("A", "B"). */
	[[nodiscard]] TArray<FName> GetBombSiteNames() const;
	/** The centre of a bomb site's volume on its floor (the volume's bottom), false without it. */
	bool GetBombSiteLocation(FName Site, FVector& OutLocation) const;
	/** Where a team spawns: its first start (level order), false without one (the bots' hunt goal). */
	bool GetTeamSpawnLocation(EShooterTeam Team, FVector& OutLocation) const;
	/** The live pawns of a team. */
	[[nodiscard]] int32 CountAlive(EShooterTeam Team) const;

	/** The loss streak of a team (the loss bonus's count). */
	[[nodiscard]] int32 GetLossStreak(EShooterTeam Team) const;
	/** The money the next loss would pay Team (with its current streak). */
	[[nodiscard]] int32 GetLossBonus(EShooterTeam Team) const;

	// The bomb's events (AShooterBomb calls them)

	void OnBombStateChanged(AShooterBomb* InBomb);
	void OnBombPlanted(AShooterBomb* InBomb, AShooterCharacter* Planter);
	void OnBombDefused(AShooterBomb* InBomb, AShooterCharacter* Defuser);
	void OnBombExploded(AShooterBomb* InBomb);

	// Buying

	/** Whether Buyer may buy now: alive, in its team's buy zone, within the buy time. */
	[[nodiscard]] bool CanBuy(const AShooterCharacter& Buyer, FString* OutReason = nullptr) const;
	/** What Item costs Buyer now (-1: not for sale to it). */
	[[nodiscard]] int32 GetPrice(const AShooterCharacter& Buyer, const FString& Item) const;
	/** Buys Item for Buyer (see the class comment); false with the reason when refused. */
	bool Buy(AShooterCharacter* Buyer, const FString& Item, FString* OutReason = nullptr);

	// Zones

	/** The trigger volume with tag Kind (and SecondTag unless NAME_None) that holds a pawn standing at Feet. */
	[[nodiscard]] static ATriggerVolume* FindZone(
		const UWorld& World, const FVector& Feet, FName Kind, FName SecondTag);
	/** A zone's name: its tag after Kind ("A", "CT"), NAME_None without one. */
	[[nodiscard]] static FName GetZoneName(const ATriggerVolume& Zone, FName Kind);

protected:
	/** Places the player in a team before its start is chosen (UE: InitNewPlayer). */
	FString InitNewPlayer(
		APlayerController* NewPlayerController, const FString& Options, const FString& Portal = FString()) override;
	/** Spawns a player unless the round is live; logs where. */
	void RestartPlayer(AController* NewPlayer) override;
	/** A new match: the scores, the money and the stats reset, then the first round (UE: HandleMatchHasStarted). */
	void HandleMatchHasStarted() override;
	void HandleMatchHasEnded() override;

private:
	/** Scores, money, stats and pawns reset, then the first round (the match's start, mp_restartgame). */
	void BeginNewMatch();
	/** Destroys what a round leaves on the map: weapons on the floor, grenades, corpses, the bomb. */
	void CleanUpMap();
	/** The team's starts, level order. */
	[[nodiscard]] TArray<AActor*> GetTeamStarts(EShooterTeam Team) const;
	/** Pays a team (every player state of it), clamped to MaxMoney. */
	void PayTeam(EShooterTeam Team, int32 Amount);
	/** Whether a round is under way (Live, or its result shown): a player joining now waits for the next. */
	[[nodiscard]] bool IsRoundLive() const;
	[[nodiscard]] float GetWorldTime() const;

	/** The number of the next bot of each team (their names: Bot_CT_1, Bot_T_1, ...). */
	int32 NextBotNumber[3] = {1, 1, 1};

	int32 NumKills = 0;

	/** Consecutive round losses of each team (EShooterTeam as the index). */
	int32 LossStreak[3] = {0, 0, 0};

	/** The terrorists' site this round (GetTerroristTargetSite). */
	FName TerroristTargetSite;

	/** The bomb was planted this round (the losing terrorists' bonus). */
	bool bBombPlantedThisRound = false;

	/** A pending mp_restartgame, and the world time it happens. */
	bool bRestartPending = false;
	float RestartGameTime = 0.0f;

	/** The round stream: the bomb's carrier. */
	FRandomStream RoundRandom;

	/** A bot match's frame: the checks, the end (see bBotMatch). */
	void TickBotMatch();
	/** Logs the match's budget numbers (the PS2 port's targets: Budgets.md). */
	void LogBotMatchBudget() const;
	FShooterMatchChecker MatchChecker;
	bool bBotMatchOver = false;
	/** The most UObjects alive in a frame of the bot match. */
	int32 BotMatchPeakObjects = 0;

	UPROPERTY(Transient)
	AShooterBomb* Bomb = nullptr;
};

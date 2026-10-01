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
class AShooterProjectile;
class AShooterSmokeCloud;
class UShooterPawnSensingComponent;
class APlayerStart;
class ATriggerVolume;

/**
 * A spot the bots watch a bomb site from (ps2-polish P3; CS's bots' hiding and approach spots): the map's waypoints
 * flagged `Lookout`, each given to its nearest site (AShooterGameMode::GetBombSiteLookouts).
 */
struct FShooterLookout
{
	/** Where the watcher stands (the waypoint's floor). */
	FVector Location = FVector::ZeroVector;
	/**
	 * The yaws a team's watcher turns between, degrees (EShooterTeam as the index): first the main way in (the first
	 * link of the waypoint graph's path to the other team's spawn), then over the site from a spot away from it and the
	 * spot's other links toward that spawn (without any, its links), at most four, 35 degrees apart or more.
	 */
	TArray<float, TInlineAllocator<4>> WatchYaws[3];

	[[nodiscard]] const TArray<float, TInlineAllocator<4>>& GetWatchYaws(EShooterTeam Team) const
	{
		return WatchYaws[static_cast<int32>(Team)];
	}
};

/**
 * ShooterGame's rules (UE ShooterGame: AShooterGameMode), the game mode of every map through GlobalDefaultGameMode
 * (plan decision D18): Counter-Strike's defusal rules, teams, rounds, money, buying and the bomb.
 *
 * Teams and spawns (P17; the team menu, ps2-polish P9):
 * - A joining player takes the team `?team=CT|T|Auto|Spectate` of the map URL names (Auto: the smaller team, CT on a
 *   tie: ChooseTeam); without one it spectates and waits for its choice (IsChoosingTeam: the player controller shows
 *   the team menu), which SelectTeam takes (CS: jointeam). A bot joins the team it is added to (bot_add_ct /
 *   bot_add_t, AddBots).
 * - A team change during a match (CS's rule): a living player dies during a fought round (a death on the scoreboard)
 *   and plays for its new side from the next round; before the round is fought (the warmup, the freeze) it respawns
 *   there at once. The bots even the teams out again at the next round's start (RebalanceBots).
 * - ChoosePlayerStart picks, in level order, the first free APlayerStart whose PlayerStartTag is the team's tag ("CT",
 *   "T"). A start is free when no pawn stands within two capsule radii of it. With every start taken it reuses the
 *   team's first; a map without team starts uses the engine's choice. A round's start places each team's players on
 *   its starts in the order they joined.
 * - The pawn stands on the start: a start's location is its capsule's centre (UE), Leon's character stands on its feet,
 *   so SpawnDefaultPawnFor lowers it by the start capsule's half height.
 *
 * Rounds (the match states of AGameMode, the round's phase in AShooterGameState):
 * - Warmup (WaitingToStart): players join and spawn at once. Once the player has a team (or spectates), NumBots bots
 *   join, shared out so the teams are as even as possible counting the player (RebalanceBots; a bot match's ten at
 *   once). The match starts (StartMatch) when both teams have a player.
 * - Each round: Freeze (FreezeTime: pawns hold still, buying), Live (RoundTime), RoundEnd (RoundRestartDelay: the
 *   result shows), each ended by the phase timer of the world's timer manager (OnPhaseTimer), then the next round or,
 * once a team has won more than half of MaxRounds or MaxRounds were played, MatchEnd (WaitingPostMatch). A round's
 * start cleans the map (weapons on the floor, grenades, corpses, the last bomb), gives the survivors their health back
 * with their weapons and armor, respawns the dead with the default inventory, and gives a new bomb to a random
 * terrorist (RandomSeed's stream, or `?seed=` in the URL). A player who joins during Live or RoundEnd waits for the
 * next round.
 * - The round ends (EndRound) when every terrorist is dead with no bomb planted, every counter-terrorist is dead, both
 *   teams die at once (a draw), the time runs out with no bomb planted (the CT win), or the planted bomb explodes (T)
 *   or is defused (CT). Planting stops the round's clock: the bomb's timer decides.
 * - Halftime (bHalftime; CS's competitive halves, CS:GO's mp_halftime): after round MaxRounds / 2 the teams switch
 * sides (HandleHalftime). Every player moves to the other team, the bots too; the scores follow the teams
 *   (AShooterGameState::BeginSecondHalf), the money goes back to StartMoney and the loss streaks to none, and every
 * pawn goes, so the second half starts as the first: everyone on the new side's starts with the default inventory. The
 *   match still ends when a team has won more than half of MaxRounds, or after MaxRounds.
 *
 * Money (Counter-Strike 1.6's, all config): StartMoney at the match's start, at most MaxMoney; a kill pays the
 * weapon's KillReward (a team kill costs TeamKillPenalty); the winners get WinReward (BombWinReward for a bomb or a
 * defuse), the losers the loss bonus, LossBonusBase growing by LossBonusIncrement a consecutive loss up to
 * LossBonusMax, and the terrorists LosingTeamPlantBonus more when they lose with the bomb planted; the planter and the
 * defuser get PlantReward / DefuseReward.
 *
 * Buying (Buy): alive, in the team's buy zone (the map's `BuyZone` volumes tagged with the team), during the freeze and
 * BuyTime after it (CS: mp_buytime counts from the freeze's end; any time in the warmup), with the money: a weapon by
 * name (glock, usp, deagle, mp5, ak47, m4a1, awp, flashbang, hegrenade, smokegrenade: its Price; a grenade already
 * carried adds one up to CS's limits, two flashbangs, one HE and one smoke; the AK-47 only for the terrorists and the
 * M4A1 only for the counter-terrorists, AShooterWeapon::BuyTeam; the same weapon twice is refused, another one in the
 * slot is dropped; it comes with a full clip and an empty reserve), `primammo` / `secammo` (a box of the primary's or
 * the pistol's ammunition, AShooterWeapon::AmmoBoxRounds for AmmoBoxPrice, while the reserve has room), `vest`
 * (kevlar), `vesthelm` (kevlar and helmet; the helmet alone with full kevlar) and `defuser` (CT only).
 *
 * The teams' buy plans (ps2-shipping N30e; CS's economy, EShooterBuyPlan): when a round starts each team decides once
 * (DecideTeamBuyPlans, ChooseBuyPlan): the pistol round (the first of each half), a full buy when half of its players
 * carry a primary or can afford the full buy (GetFullBuyCost: the team's rifle and kevlar with a helmet), else a
 * force-buy after ForceBuyLossStreak losses in a row, after a win and in a half's last round, and an eco otherwise. The
 * bots buy by it (AShooterAIController::BuyForRound).
 *
 * The radio (ps2-shipping N30e; CS 1.6's): SendRadioMessage puts a player's message (EShooterRadioMessage) in the game
 * state's radio log for its team's HUDs, at most every RadioCooldown and MaxRadioMessagesPerRound a round (a grenade's
 * "Fire in the hole!" and a bot's "Bomb has been planted." regardless), and its team's bots hear it; the bot nearest
 * the sender answers a request.
 *
 * Bots: named from BotNames (CS's BotProfile names, team-neutral) in the order they are created, and seeded by that
 * order (AShooterAIController::SetBotIndex), not by their names. Each takes BotDifficulty's skill preset
 * (AShooterAIController::ApplyDifficulty).
 *
 * The main menu's match (ps2-polish P9; FShooterMatchSettings): InitGame reads `?bots=N` (NumBots), `?difficulty=`
 * (BotDifficulty: Easy, Normal, Hard, Expert) and `?winrounds=N` (MaxRounds = 2 N - 1: the first team to N wins, the
 * halftime after round N - 1).
 *
 * Damage: CanDealDamage refuses a teammate's (bFriendlyFire false, CS's mp_friendlyfire 0); a player may hurt itself
 * (its own grenade). Killed hears of each death from AShooterCharacter::Die: the kill feed, the money, the kills and
 * deaths. Whether the round is over is checked on the next tick, so the deaths of one moment (an explosion that kills
 * the last of both teams) end it together (a draw).
 *
 * Registries (ps2-shipping N20: the EE cannot walk the level's actors for every bot every frame): the game mode keeps
 * the map's trigger volumes (the bomb sites, the buy zones, the ladders) and player starts, found in the level when it
 * is made and added as they spawn (UWorld::AddOnActorSpawnedHandler), and the game's actors, which join when they
 * begin play and leave when they end it (the shooter pawns, the bombs, the projectiles) or while they lie on the floor
 * (the pickups: dropped weapons, the dropped bomb). Each keeps the level's order, so what walked the level before finds
 * the same actors in the same order. The bomb sites (by name, with their places) and each team's starts are sorted out
 * once, when a volume or a start joins or goes. A world without a ShooterGameMode has no registries: nothing is picked
 * up there. The bots' lookouts of each site (GetBombSiteLookouts) are made once from the waypoint graph.
 *
 * Console (the Exec chain reaches the game mode): `bot_add_ct [N]`, `bot_add_t [N]`, `bot_add [N]` (the smaller team),
 * `bot_fill` (both teams to MaxPlayersPerTeam; the G6 smoke: `ShooterGame -nullrhi -ExecCmds=bot_fill`),
 * `bot_kick [name|all]`, `bot_stop [0|1]` (the bots freeze), `mp_restartgame [seconds]` (a new match after that
 * many seconds, 1 by default), `mp_maxrounds [N]` (MaxRounds, from the next round's end on) and `mp_halftime [0|1]`
 * (bHalftime).
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
	/** A ladder's volume (UShooterCharacterMovement's ladders; a map's Ladder node). */
	static const FName LadderTag;

	/**
	 * The players a team takes (CS: 5 a side); more bots are refused. A match of more bots than that (`?bots=N`) grows
	 * it to take them and the player, and `?teamsize=N` or `-teamsize=N` (a larger bot match: the stress test) sets
	 * it; never past MaxTeamSize.
	 */
	UPROPERTY(Config)
	int32 MaxPlayersPerTeam = 5;

	/** The most players a team can have: the maps' starts a team (sixteen in ShooterGame's maps). */
	UPROPERTY(Config)
	int32 MaxTeamSize = 16;

	/** Teammates hurt each other (CS: mp_friendlyfire). */
	UPROPERTY(Config)
	bool bFriendlyFire = false;

	/** The bots stand still and do nothing (CS: bot_stop 1; the `bot_stop [0|1]` command). */
	UPROPERTY(Config)
	bool bBotStop = false;

	/**
	 * The bots of the match (CS: bot_quota), shared between the teams around the humans once the player has chosen a
	 * team (RebalanceBots); `?bots=N` in the URL. A bot match has 2 x MaxPlayersPerTeam.
	 */
	UPROPERTY(Config)
	int32 NumBots = 9;

	/** The skill of the bots the game mode adds (`?difficulty=` in the URL; AShooterAIController::ApplyDifficulty). */
	UPROPERTY(Config)
	EShooterBotDifficulty BotDifficulty = EShooterBotDifficulty::Normal;

	/**
	 * The most bots that look (their sight's traces) in one frame (ClaimSensingUpdate); the others wait for the next
	 * frame. 0: no limit.
	 */
	UPROPERTY(Config)
	int32 MaxSensingUpdatesPerFrame = 2;

	/**
	 * Seconds of each phase (CS: mp_freezetime, mp_roundtime, the round restart delay) and of buying after the freeze
	 * (mp_buytime; the freeze itself is for buying too).
	 */
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

	/** The teams switch sides after half of MaxRounds (CS:GO: mp_halftime; see the class comment). */
	UPROPERTY(Config)
	bool bHalftime = true;

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

	/**
	 * The team's buy plan (ps2-shipping N30e, GetTeamBuyPlan): the consecutive losses after which a team that cannot
	 * afford the full buy spends what it has anyway (a force-buy) instead of saving.
	 */
	UPROPERTY(Config)
	int32 ForceBuyLossStreak = 2;

	/** The radio (CS 1.6): seconds between two messages of a player, and the most a player sends in a round. */
	UPROPERTY(Config)
	float RadioCooldown = 1.5f;

	UPROPERTY(Config)
	int32 MaxRadioMessagesPerRound = 60;

	/** The seed of the round stream (the bomb's carrier); `?seed=N` in the URL or `-seed=N` overrides it. */
	UPROPERTY(Config)
	int32 RandomSeed = 1;

	/**
	 * The bots' names (ps2-shipping N30e: CS 1.6's BotProfile names, team-neutral, since a bot keeps its name when the
	 * halftime moves it to the other side), given in the order the bots are created (GetBotName). DefaultGame.ini's
	 * +BotNames.
	 */
	UPROPERTY(Config)
	TArray<FString> BotNames;

	/**
	 * The name of the bot created BotIndex-th (0 the first): BotNames in order, and past the list's end its names again
	 * with the round of the list ("Albert (2)"); "Bot <n>" without a list.
	 */
	[[nodiscard]] FString GetBotName(int32 BotIndex) const;

	/** The class of the bots' controllers. */
	UPROPERTY()
	TSubclassOf<AShooterAIController> BotControllerClass;

	/** The class of the bomb (AShooterBomb). */
	UPROPERTY()
	TSubclassOf<AShooterBomb> BombClass;

	/**
	 * A headless bot match (plan P21; `-botmatch [-rounds=N] [-seed=N]` on the command line): the local player
	 * spectates, bots fill both teams, FShooterMatchChecker checks every frame, and after BotMatchRounds rounds (or
	 * the match's end, or a deadline for them) the game exits with 0, or 1 when an invariant broke. The match is
	 * BotMatchRounds long (MaxRounds, so the teams switch sides at its half). Run it with -nullrhi -benchmark to play
	 * faster than real time.
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
	/** Finds the level's trigger volumes and player starts, and listens for the ones spawned later (the registries). */
	void PostInitializeComponents() override;
	/** Logs how many pawns each team has when the match leaves (the G6 smoke reads it). */
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/**
	 * Adds up to Count bots to Team (None: the smaller team each time); returns how many joined (a full team refuses
	 * the rest). A bot spawns at once unless the round is live (it waits for the next one). Each bot takes a sensing
	 * slot of 2 x MaxPlayersPerTeam (AShooterAIController::SetSensingSlot), the teams interleaved (CT 0, T 1, CT 2,
	 * ...): the bots look in turn.
	 */
	int32 AddBots(EShooterTeam Team, int32 Count);

	/** Adds bots until both teams have MaxPlayersPerTeam (CS: bot_fill; plan P19's 5v5 fill); how many joined. */
	int32 FillTeamsWithBots();

	/**
	 * How NumBots bots split between the teams so that they are as even as possible counting the humans on each
	 * (ps2-polish P9): half of all the players a side, the odd one to the side with fewer humans (the player's
	 * opponents; T on a tie), never fewer than a side's humans nor more than MaxPerTeam.
	 */
	static void ComputeBotSplit(
		int32 InNumBots, int32 HumansCT, int32 HumansT, int32 MaxPerTeam, int32& OutBotsCT, int32& OutBotsT);

	/**
	 * Shares NumBots bots out between the teams (ComputeBotSplit around the humans): the bots too many on a side move
	 * to the other (the last to join first, their pawns gone until the round's start), then those still too many
	 * leave, then the missing ones join (CT first). The player's team choice asks for it (at once in the warmup and
	 * the freeze, else at the next round's start).
	 */
	void RebalanceBots();

	/**
	 * The player's team choice (the team menu; CS: jointeam): CT, T, Auto (ChooseTeam without it) or Spectate. The
	 * first choice lets the bots join; a later one changes the player's side by CS's rule (see the class comment).
	 * False when nothing changed (the team it has) or Player is not a human's.
	 */
	bool SelectTeam(AController* Player, EShooterTeamChoice Choice);

	/** Player joined without a team and waits for its choice (the team menu). */
	[[nodiscard]] bool IsChoosingTeam(const AController* Player) const;

	/** The rounds a team needs to win the match: more than half of MaxRounds. */
	[[nodiscard]] int32 GetRoundsToWin() const
	{
		return (MaxRounds / 2) + 1;
	}

	/** Removes a bot by player name ("all": every bot); how many left. */
	int32 KickBots(const FString& Name);

	/** The team a new player joins: `?team=` of Options, else the smaller team, CT on a tie. */
	[[nodiscard]] EShooterTeam ChooseTeam(const FString& Options) const;

	/** The team a choice gives Player's state: the smaller team without it for Auto, None for Spectate. */
	[[nodiscard]] EShooterTeam ResolveTeamChoice(EShooterTeamChoice Choice, const AShooterPlayerState* Player) const;

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

	// Registries (see the class comment)

	/** The shooter pawns that began play and have not ended it, in the level's order. */
	[[nodiscard]] const TArray<AShooterCharacter*>& GetPawns() const
	{
		return Pawns;
	}
	void RegisterPawn(AShooterCharacter* Pawn);
	void UnregisterPawn(AShooterCharacter* Pawn);
	/**
	 * A bot asks to look now (UShooterPawnSensingComponent): the bots whose look is due queue in the order they asked,
	 * and the first MaxSensingUpdatesPerFrame of the queue look in a frame; true when Sensor may look (it leaves the
	 * queue), false when it waits for a later frame (its place kept, so nobody waits for ever).
	 */
	bool ClaimSensingUpdate(UShooterPawnSensingComponent* Sensor);
	/** Sensor leaves the queue of looks (its bot died, froze or left). */
	void CancelSensingUpdate(UShooterPawnSensingComponent* Sensor);
	/** The looks taken in the current frame. */
	[[nodiscard]] int32 GetSensingUpdatesThisFrame() const;

	/** A pawn died, changed hands or teams: CountAlive counts again (else it keeps its count for the frame). */
	void NotifyPawnsChanged()
	{
		++PawnsSerial;
	}

	/** What lies on the floor to be picked up (dropped weapons, the dropped bomb), in the order it fell. */
	[[nodiscard]] const TArray<AActor*>& GetPickups() const
	{
		return Pickups;
	}
	void RegisterPickup(AActor* Pickup);
	void UnregisterPickup(AActor* Pickup);

	/** The bombs in play (carried, dropped or planted). */
	[[nodiscard]] const TArray<AShooterBomb*>& GetBombs() const
	{
		return Bombs;
	}
	void RegisterBomb(AShooterBomb* InBomb);
	void UnregisterBomb(AShooterBomb* InBomb);

	/** The grenades in flight. */
	void RegisterProjectile(AShooterProjectile* Projectile);
	void UnregisterProjectile(AShooterProjectile* Projectile);

	/** The smoke grenades' clouds (the round's clean-up removes them). */
	[[nodiscard]] const TArray<AShooterSmokeCloud*>& GetSmokeClouds() const
	{
		return SmokeClouds;
	}
	void RegisterSmokeCloud(AShooterSmokeCloud* Cloud);
	void UnregisterSmokeCloud(AShooterSmokeCloud* Cloud);
	/** A thick smoke cloud lies across the line from Start to End (the bots' sight: AShooterSmokeCloud::BlocksLine). */
	[[nodiscard]] bool IsSightBlockedBySmoke(const FVector& Start, const FVector& End) const;

	/** The map's trigger volumes (any tag) and player starts, in the level's order. */
	[[nodiscard]] const TArray<ATriggerVolume*>& GetZones() const
	{
		return Zones;
	}
	[[nodiscard]] const TArray<APlayerStart*>& GetPlayerStarts() const
	{
		return PlayerStarts;
	}
	/** A trigger volume or a player start joins its registry (once); any other actor is ignored. */
	void RegisterMapActor(AActor* Actor);

	// Rounds

	/** The game state, as the game's class. */
	[[nodiscard]] AShooterGameState* GetShooterGameState() const;
	/** Both teams have a player (UE: ReadyToStartMatch). */
	[[nodiscard]] virtual bool ReadyToStartMatch() const;
	/** Cleans the map, places and respawns the players, gives out the bomb and starts the freeze. */
	void StartRound();
	/** The round is over: the score, the money, the message; the next round after RoundRestartDelay. */
	void EndRound(EShooterRoundEndReason Reason);
	/** Checks the eliminations (each tick of a live round, and when its time runs out). */
	void CheckRoundEnd();
	/**
	 * The round's phase is over (its timer): the freeze goes live, a live round's time runs out (unless the bomb is
	 * planted: then the bomb decides), the result gives way to the next round or the match's end.
	 */
	void OnPhaseTimer();
	/** A new match from the first round after Delay seconds (mp_restartgame). */
	void RestartGame(float Delay);
	/** The round after which the teams switch sides: MaxRounds / 2 with bHalftime, else 0 (none). */
	[[nodiscard]] int32 GetHalftimeRound() const
	{
		return bHalftime ? MaxRounds / 2 : 0;
	}
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
	[[nodiscard]] const TArray<FName>& GetBombSiteNames() const;
	/** The centre of a bomb site's volume on its floor (the volume's bottom), false without it. */
	bool GetBombSiteLocation(FName Site, FVector& OutLocation) const;
	/** Where a team spawns: its first start (level order), false without one (the bots' hunt goal). */
	bool GetTeamSpawnLocation(EShooterTeam Team, FVector& OutLocation) const;
	/**
	 * The spots the bots watch Site from (FShooterLookout; ps2-polish P3): the waypoints flagged `Lookout` nearest to
	 * it; a map without them, the site's nearest waypoints (up to three within LookoutFallbackRadius), else its middle,
	 * watching around from the other team's side. Made once from the world's waypoint graph (again when the sites or
	 * the graph change); empty for a site the map does not have.
	 */
	[[nodiscard]] const TArray<FShooterLookout>& GetBombSiteLookouts(FName Site) const;

	/** A map without lookouts: how near a site its waypoints stand in for them, cm. */
	UPROPERTY(Config)
	float LookoutFallbackRadius = 1500.0f;
	/**
	 * The live pawns of a team (CountPawns), counted once a frame: the count holds until the world's time moves on or
	 * NotifyPawnsChanged.
	 */
	[[nodiscard]] int32 CountAlive(EShooterTeam Team) const;

	/** The loss streak of a team (the loss bonus's count). */
	[[nodiscard]] int32 GetLossStreak(EShooterTeam Team) const;
	/** The money the next loss would pay Team (with its current streak). */
	[[nodiscard]] int32 GetLossBonus(EShooterTeam Team) const;

	// The economy's plan (ps2-shipping N30e)

	/**
	 * A team's buy plan (CS's economy, EShooterBuyPlan): the pistol round (the first of a half) buys armor; a team
	 * with at least half of its players equipped or able to buy in full (the team's rifle and kevlar with a helmet)
	 * buys in full; else it force-buys after ForceBuyLossStreak losses in a row, after a win (a winner keeps
	 * spending) and in the last round of a half, and saves otherwise.
	 */
	[[nodiscard]] static EShooterBuyPlan ChooseBuyPlan(bool bPistolRound, bool bLastRoundOfHalf, int32 LossStreak,
		int32 NumEquipped, int32 TeamSize, int32 InForceBuyLossStreak);
	/** The plan Team decided when the round started (the freeze): its bots buy by it. */
	[[nodiscard]] EShooterBuyPlan GetTeamBuyPlan(EShooterTeam Team) const
	{
		return TeamBuyPlan[static_cast<int32>(Team)];
	}
	/** What the full buy costs Buyer: its team's rifle and kevlar with a helmet (the AK-47's $3500, the M4A1's $4100).
	 */
	[[nodiscard]] int32 GetFullBuyCost(const AShooterCharacter& Buyer) const;
	/** Buyer carries a primary or can afford the full buy with Money. */
	[[nodiscard]] bool IsEquippedOrCanFullBuy(const AShooterCharacter& Buyer, int32 Money) const;

	// The radio (ps2-shipping N30e)

	/**
	 * Sender radios Message to its team (CS 1.6's radio): a living player on a team, at most every RadioCooldown and
	 * MaxRadioMessagesPerRound times a round (a grenade's "Fire in the hole!" and a bot's "Bomb has been planted." go
	 * out regardless). Location is where it is about (the enemy spotted), else where the sender stands. The game state
	 * keeps it for the HUD (the team's lines), and every bot of the team hears it
	 * (AShooterAIController::OnRadioMessage); a request (IsRadioRequest) is answered by the living bot of the team
	 * nearest the sender. True when sent.
	 */
	bool SendRadioMessage(AController* Sender, EShooterRadioMessage Message, const FVector* Location = nullptr);

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

	/**
	 * The registered trigger volume with tag Kind (and SecondTag unless NAME_None) that holds a pawn standing at Feet,
	 * the first in level order.
	 */
	[[nodiscard]] ATriggerVolume* FindZone(const FVector& Feet, FName Kind, FName SecondTag) const;
	/** A zone's name: its tag after Kind ("A", "CT"), NAME_None without one. */
	[[nodiscard]] static FName GetZoneName(const ATriggerVolume& Zone, FName Kind);
	/**
	 * The registered ladder (a trigger volume tagged LadderTag) that a capsule of Radius and Height standing at Feet
	 * touches, the first in level order: its box grown by the radius across, and the feet below its top.
	 */
	[[nodiscard]] ATriggerVolume* FindLadder(const FVector& Feet, float Radius, float Height) const;

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
	/** The teams switch sides (see the class comment); the next round starts the second half. */
	void HandleHalftime();
	/** Every shooter pawn goes (its controller lets it go): the next round respawns everyone with the default
	 * inventory. */
	void DestroyAllPawns();
	/** Destroys what a round leaves on the map: weapons on the floor, grenades, corpses, the bomb. */
	void CleanUpMap();
	/** The team's starts, level order. */
	[[nodiscard]] const TArray<APlayerStart*>& GetTeamStarts(EShooterTeam Team) const;
	/** Sorts out the bomb sites and the team starts again when a zone or a start joined or went. */
	void UpdateMapCaches() const;
	/** The world's spawn handler: a spawned trigger volume or player start joins its registry. */
	void OnActorSpawned(AActor* Actor);
	/** Pays a team (every player state of it), clamped to MaxMoney. */
	void PayTeam(EShooterTeam Team, int32 Amount);
	/** Whether a round is under way (Live, or its result shown): a player joining now waits for the next. */
	[[nodiscard]] bool IsRoundLive() const;
	[[nodiscard]] float GetWorldTime() const;

	/**
	 * How many bots the game mode has created (the next one's index: its name and its random stream), and how many
	 * joined each team (their sensing slots; EShooterTeam as the index).
	 */
	int32 NumBotsCreated = 0;
	int32 NumBotsAddedToTeam[3] = {0, 0, 0};

	/** A team choice (or a bot match) asks the bots to share out (RebalanceBots) at the next chance. */
	bool bRebalancePending = false;
	/** The players who joined without a team and wait for their choice (IsChoosingTeam). */
	TArray<TWeakObjectPtr<AController>> PlayersChoosingTeam;
	/** A bot goes to Team (RebalanceBots): its pawn goes (it respawns there at the round's start). */
	void MoveBotToTeam(AShooterAIController& Bot, EShooterTeam Team);
	/** A bot leaves quietly (RebalanceBots: no death on the board). */
	void RemoveBot(AShooterAIController& Bot);

	/** The world time of the frame whose looks are counted, and how many were taken (ClaimSensingUpdate). */
	float SensingFrameTime = -1.0f;
	int32 SensingUpdatesThisFrame = 0;

	/** The bots waiting to look, in the order they asked (ClaimSensingUpdate). */
	UPROPERTY(Transient)
	TArray<UShooterPawnSensingComponent*> SensingQueue;

	int32 NumKills = 0;

	/** Consecutive round losses of each team (EShooterTeam as the index). */
	int32 LossStreak[3] = {0, 0, 0};

	/** Each team's buy plan for the round (EShooterTeam as the index), decided when it starts. */
	EShooterBuyPlan TeamBuyPlan[3] = {EShooterBuyPlan::Pistol, EShooterBuyPlan::Pistol, EShooterBuyPlan::Pistol};
	/** Decides both teams' buy plans (the round's start, once the players are placed). */
	void DecideTeamBuyPlans();

	/** The terrorists' site this round (GetTerroristTargetSite). */
	FName TerroristTargetSite;

	/** The bomb was planted this round (the losing terrorists' bonus). */
	bool bBombPlantedThisRound = false;

	/** The timer of the round's phase (OnPhaseTimer), of the buy time (CanBuy) and of a pending mp_restartgame. */
	FTimerHandle TimerHandle_Phase;
	FTimerHandle TimerHandle_BuyTime;
	FTimerHandle TimerHandle_RestartGame;
	/** A pending mp_restartgame comes (RestartGame's timer). */
	void OnRestartGameTimer();
	/** Sets the phase timer to Seconds. */
	void SetPhaseTimer(float Seconds);

	/** The round stream: the bomb's carrier. */
	FRandomStream RoundRandom;

	/**
	 * Asks for the assets the game spawns later (Docs/PLANS/ps2-shipping.md N24): every soft object path of the
	 * weapons', their projectiles', the pawn's, the bomb's and the player controller's class defaults (in structs and
	 * arrays too), by LoadPackageAsync. The map's load ends with them (UEngine::LoadMap flushes), so no frame reads the
	 * disc; PreloadedAssets keeps them loaded while the map plays (a round's collection would drop a weapon nobody
	 * holds).
	 */
	void RequestGameplayAssets();
	/** Asks for Path's package unless it was asked for or does not exist; true when it asked. */
	bool RequestPreloadPath(const FSoftObjectPath& Path);
	/** A preloaded package came in: its assets join PreloadedAssets. */
	void OnGameplayPackageLoaded(const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result);

	/** The soft object paths RequestGameplayAssets asked for, by package. */
	TMap<FName, TArray<FSoftObjectPath>> PreloadPaths;
	/** Their LoadPackageAsync requests (EndPlay flushes what is still in flight). */
	TArray<int32> PreloadRequestIds;

	/** The preloaded assets (RequestGameplayAssets). */
	UPROPERTY(Transient)
	TArray<UObject*> PreloadedAssets;

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

	// The registries (see the class comment)

	UPROPERTY(Transient)
	TArray<AShooterCharacter*> Pawns;

	UPROPERTY(Transient)
	TArray<AActor*> Pickups;

	UPROPERTY(Transient)
	TArray<AShooterBomb*> Bombs;

	UPROPERTY(Transient)
	TArray<AShooterProjectile*> Projectiles;

	UPROPERTY(Transient)
	TArray<AShooterSmokeCloud*> SmokeClouds;

	UPROPERTY(Transient)
	TArray<ATriggerVolume*> Zones;

	UPROPERTY(Transient)
	TArray<APlayerStart*> PlayerStarts;

	/** The world's spawn handler (OnActorSpawned). */
	FDelegateHandle ActorSpawnedHandle;

	/**
	 * What UpdateMapCaches sorts out of Zones and PlayerStarts (which hold their actors): the bomb sites by name with
	 * their places and volumes, and each team's starts in level order (EShooterTeam as the index).
	 */
	mutable TArray<FName> BombSiteNames;
	mutable TArray<FVector> BombSiteLocations;
	mutable TArray<ATriggerVolume*> BombSiteZones;
	mutable TArray<APlayerStart*> TeamStarts[3];
	mutable bool bMapCachesDirty = true;

	/** Makes the sites' lookouts (GetBombSiteLookouts) from the world's waypoint graph. */
	void BuildBombSiteLookouts() const;
	/**
	 * Each site's lookouts (BombSiteNames' order), the waypoint graph's node count they were made from, and whether the
	 * sites changed since.
	 */
	mutable TArray<TArray<FShooterLookout>> BombSiteLookouts;
	mutable int32 LookoutsNodeCount = -1;
	mutable bool bLookoutsDirty = true;

	/** CountAlive's count (EShooterTeam as the index), with the world time and the pawns' serial it was counted at. */
	uint32 PawnsSerial = 0;
	mutable uint32 AliveCountSerial = 0;
	mutable float AliveCountTime = -1.0f;
	mutable int32 AliveCount[3] = {0, 0, 0};
};

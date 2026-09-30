#pragma once

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "ShooterTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "ShooterAIController.generated.h"

class AShooterBomb;
class AShooterCharacter;
class AShooterGameMode;
class AShooterWeapon;
class AShooterWeapon_Knife;
class AShooterWeapon_Projectile;
class UShooterPawnSensingComponent;
struct FShooterLookout;
struct FShooterRadioEntry;

/**
 * A bot's controller (UE ShooterGame: AShooterAIController, whose bots run a behavior tree asset): Counter-Strike's
 * bot on Leon's behavior tree (a C++ tree over a typed blackboard), senses (UShooterPawnSensingComponent: sight, and
 * the shots it hears) and the waypoint navigation (AAIController's MoveTo*).
 *
 * Sight (ps2-shipping N20): a bot looks every SensingInterval (0.1 s) at the living enemies only
 * (UShooterPawnSensingComponent), and the bots take turns: the game mode spreads their looks over the interval
 * (SetSensingSlot, the teams interleaved) and lets at most MaxSensingUpdatesPerFrame of them look in a frame (a look
 * over the budget waits for the next frame). At 60 fps no look waits, so a bot reacts as before; only the moment of
 * its looks moved.
 *
 * The tree, most urgent first (each tick):
 * 1. Frozen (the freeze time), dead, or stopped (bot_stop): stand; in the freeze, buy once by the team's plan
 *    (BuyForRound). Blinded by a flashbang (AShooterCharacter::IsBlind): it sees nobody, stands, and fires at random
 *    around where it last saw an enemy (else around its view) until the blindness passes (the Blind branch; CS's bots).
 * 2. A grenade throw under way (ThrowGrenade): draw it, turn to the throw and throw (an enemy that shows up meanwhile
 *    waits: CS's bots throw anyway); after a flashbang, turn its back to it until it has gone off, unless an enemy is
 *    in sight. The throws (ConsiderGrenade, a few times a second, no enemy in sight): a terrorist nearing the round's
 *    site (a counter-terrorist the planted bomb) within EntryGrenadeDistance throws its flashbang (else its smoke
 *    grenade; a CT its HE) at it once a round, when the round's draw (GrenadeChance) says so; an enemy seen or heard
 *    between MinGrenadeDistance and MaxGrenadeDistance away (or reported by a teammate) gets the HE (else a flashbang,
 *    else the smoke), again by a draw a spot. The aim point is off by up to GrenadeThrowError, the pitch the throw's
 * low arc; a throw that would hit a wall at the bot's nose waits.
 * 3. An enemy in sight: engage (Engage) with the best weapon that has ammunition (never a grenade: the throws are
 *    the bot's own). Turn to it at AimTurnRate, and fire once ReactionTime has passed since it came into sight: the aim
 *    is off by an error that starts at AimError and shrinks with the time on target (AimErrorDecayTime), drawn again
 *    for each burst; automatic weapons fire bursts of BurstShots with BurstPause between; the AWP zooms first. It moves
 *    as CS's bots do: it strafes left and right (StrafeMinTime to StrafeMaxTime each way, from its stream), crouches
 *    with a rifle at CrouchFireDistance or farther, and stands still with the AWP. With the knife it rushes (ps2-polish
 *    P3): it runs the path to the enemy (a new one as the enemy moves), closes in by the side once within the slash's
 *    reach, and cuts only within reach: a stab in the back when the enemy turns it, a slash otherwise. An enemy out of
 *    sight for EnemyMemory seconds is forgotten (its last place is searched). The weapon's recoil kicks the aim as it
 *    kicks a player's: the bot turns its own aim to the target and the kick rides on top, the bot pulling
 *    RecoilCompensation of each new kick back down.
 * 4. A counter-terrorist and the bomb planted: the retake (ps2-polish P3b). Gather at a staging point toward the CT
 *    spawn (RetakeStagingDistance) until a teammate is there or RetakeWaitTime passes, go in, and defuse (hold the use
 *    key) once no enemy was seen for SiteClearTime, or at once when the time is short. Outnumbered by
 *    RetakeGiveUpAdvantage or too late to defuse, it gives the retake up ("Team, fall back!") and saves itself at its
 *    spawn (the objective).
 * 5. The bomb's carrier: go to the round's site (AShooterGameMode::GetTerroristTargetSite) and plant once inside.
 * 6. A terrorist and the bomb dropped: fetch it.
 * 7. A weapon on the floor worth the walk (PickUp): out of ammunition, the nearest dropped weapon with ammunition it
 *    may take within twice PickupSearchDistance; without a loaded primary, the nearest primary within
 *    PickupSearchDistance. Its spent weapon is dropped for it (AShooterWeapon::CanBePickedUpBy).
 * 8. A terrorist without the bomb while the carrier pushes (still SupportDistance or farther from the site): escort
 *    it, watching to one side of its way.
 * 9. A shot heard (an enemy's), or a teammate's report: go and look where it came from.
 * 10. Hunt: outnumbering the enemy by HuntAdvantage, or a terrorist with HuntTimeLeft left and no bomb planted: to the
 *    enemy's spawn, then from waypoint to waypoint of the map (drawn from the stream) until contact.
 * 11. The objective (ps2-polish P3, CS's bots with no enemy in sight): the counter-terrorists hold a site each (A for
 *    the even ones, B for the odd, by their place in the team) and the terrorists take the round's site (its support
 *    spots once the carrier is there; the planted bomb's site to guard it: HoldPlantedBomb, P3b). Either moves between
 * the site's lookouts (AShooterGameMode::GetBombSiteLookouts), LookoutMinTime to LookoutMaxTime at each, turning
 * between the lookout's watched directions (the ways in) every WatchMinTime to WatchMaxTime. A counter-terrorist that
 * held its site RotateTime with no contact rotates to the next one, and one that hears a teammate's enemy at another
 * site goes there with RotateOnReportChance.
 *
 * On the move with nobody to aim at, a bot looks along its path (LookTurnRate). Ladders (ps2-polish P3): the waypoint
 * graph links a ladder's foot and top, and on a ladder the bot faces it and climbs up (the path's point above) or
 * down (looking down), and jumps off it at the bottom.
 *
 * The radio (ps2-shipping N30e; AShooterGameMode::SendRadioMessage): a bot says "Enemy spotted." with the enemy's
 * place when it sees a new one, "Need backup." once a round when its health falls below NeedBackupHealth, "Sector
 * clear." when a place it went to look at is empty, "Bomb has been planted." (the game mode) and "Fire in the hole!"
 * (the grenade); none of them when a teammate said it within RadioRepeatTime. It hears its team: a teammate's "Enemy
 * spotted." within RadioReportRange is a place to look at (as a heard shot) for a bot that fights nobody, has nothing
 * else to look at and holds no site, and the bot nearest a request answers it ("Affirmative.", "Reporting in.") and,
 * for "Need backup." or "Taking fire", goes there.
 *
 * Difficulty (ps2-polish P9; CS's bot_difficulty and bot profiles): the game mode gives each new bot the preset of the
 * match's difficulty (ApplyDifficulty; DifficultyPresets in the config, one for Easy, Normal, Hard and Expert): its
 * reaction time, aim error, turn rate, recoil control and memory. A bot made without the game mode keeps the class's
 * values, Normal's. Every random choice (the aim error, the AWP, the strafes,
 * the grenades, the blind fire) comes from the bot's stream, seeded from the game mode's RandomSeed and the bot's
 * index (the order the game mode created the bots in: SetBotIndex), so a match with a seed replays, and a bot's name
 * (CS's team-neutral BotProfile names, AShooterGameMode::BotNames) never changes it.
 */
UCLASS(Config = Game)
class SHOOTERGAME_API AShooterAIController : public AAIController
{
	GENERATED_BODY()

public:
	AShooterAIController(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	/** The blackboard's keys. */
	static const FName EnemyKey;
	static const FName HasEnemyKey;
	static const FName ShouldDefuseKey;
	static const FName CarriesBombKey;
	static const FName BombDroppedKey;
	static const FName HeardEnemyKey;
	static const FName NoiseLocationKey;
	static const FName ShouldEscortKey;
	static const FName ShouldHuntKey;
	static const FName IsBlindKey;
	static const FName IsThrowingKey;
	static const FName ShouldPickUpKey;

	/**
	 * The skill of each difficulty (see the class comment; DefaultGame.ini's `+DifficultyPresets`): ApplyDifficulty
	 * copies the preset into the values below.
	 */
	UPROPERTY(Config)
	TArray<FShooterBotSkill> DifficultyPresets;

	/**
	 * Takes the skill of InDifficulty's preset (the game mode, when it adds the bot); a difficulty without a preset
	 * leaves the bot as it is (a warning). False then.
	 */
	bool ApplyDifficulty(EShooterBotDifficulty InDifficulty);
	/** The difficulty the bot took (Normal until ApplyDifficulty). */
	[[nodiscard]] EShooterBotDifficulty GetDifficulty() const
	{
		return Difficulty;
	}

	// The skill (the difficulty's preset: FShooterBotSkill; Normal's values until a preset is applied)

	/** Seconds from an enemy coming into sight to the first shot. */
	UPROPERTY()
	float ReactionTime = 0.35f;

	/** The aim's error on a new target, degrees, and how fast it settles (seconds for 1/e) down to MinAimError. */
	UPROPERTY()
	float AimError = 5.0f;

	UPROPERTY()
	float AimErrorDecayTime = 0.8f;

	UPROPERTY()
	float MinAimError = 0.4f;

	/** How far the bot sees an enemy, cm (its senses' SightRadius). */
	UPROPERTY(Config)
	float SightRadius = 3500.0f;

	/** How fast the bot turns to its aim, degrees a second. */
	UPROPERTY()
	float AimTurnRate = 360.0f;

	/**
	 * The part of each recoil kick the bot pulls back down as it fires, 0 (the kick climbs as it does for a player who
	 * does not pull down) to 1 (none shows: a laser).
	 */
	UPROPERTY()
	float RecoilCompensation = 0.5f;

	/** Shots a burst of an automatic weapon, and the pause after it, seconds. */
	UPROPERTY(Config)
	int32 BurstShots = 4;

	UPROPERTY(Config)
	float BurstPause = 0.35f;

	/** Seconds an enemy out of sight is remembered. */
	UPROPERTY()
	float EnemyMemory = 1.5f;

	/** The chance to buy the AWP when it can be afforded with armor. */
	UPROPERTY(Config)
	float AwpChance = 0.2f;

	/** How near a goal counts as there, cm. */
	UPROPERTY(Config)
	float GoalReachedDistance = 200.0f;

	/** How near the bomb carrier an escorting terrorist stays, cm. */
	UPROPERTY(Config)
	float EscortDistance = 350.0f;

	/** Seconds a CT holds its site without contact before it rotates to the next one. */
	UPROPERTY(Config)
	float RotateTime = 25.0f;

	/** The living players a team needs over the enemy's to hunt them (no bomb planted); 0 never hunts. */
	UPROPERTY(Config)
	int32 HuntAdvantage = 2;

	/** The round's seconds left below which a terrorist with no bomb planted hunts; 0 never. */
	UPROPERTY(Config)
	float HuntTimeLeft = 30.0f;

	// No enemy in sight (ps2-polish P3)

	/** Seconds at a lookout before the next one of the site, drawn between these. */
	UPROPERTY(Config)
	float LookoutMinTime = 3.0f;

	UPROPERTY(Config)
	float LookoutMaxTime = 7.0f;

	/** Seconds watching one direction of a lookout before turning to the next, drawn between these. */
	UPROPERTY(Config)
	float WatchMinTime = 1.0f;

	UPROPERTY(Config)
	float WatchMaxTime = 2.5f;

	/** How fast the bot turns to look (along its path, a lookout's directions), degrees a second. */
	UPROPERTY(Config)
	float LookTurnRate = 240.0f;

	/** How near its site the carrier must be for its escorts to take the site's support spots, cm. */
	UPROPERTY(Config)
	float SupportDistance = 1500.0f;

	/** A teammate's enemy this near a site is at that site (a counter-terrorist may rotate there), cm. */
	UPROPERTY(Config)
	float SiteReportRadius = 1500.0f;

	/** The chance a counter-terrorist holding a site rotates to another site where a teammate saw an enemy. */
	UPROPERTY(Config)
	float RotateOnReportChance = 1.0f;

	/** How far a bot without a loaded primary goes for one on the floor, cm (out of ammunition: twice, any weapon). */
	UPROPERTY(Config)
	float PickupSearchDistance = 1500.0f;

	// The planted bomb (ps2-polish P3b)

	/**
	 * The terrorists hold the planted bomb from the lookouts this near it (else from the bomb), and look after nothing
	 * heard or reported farther from it, cm.
	 */
	UPROPERTY(Config)
	float PostPlantHoldRadius = 1000.0f;

	/** The counter-terrorists' retake: they gather this far from the bomb, toward their spawn, before going in, cm. */
	UPROPERTY(Config)
	float RetakeStagingDistance = 1500.0f;

	/** How near the staging point a teammate counts as gathered, cm, and the longest wait for one there, s. */
	UPROPERTY(Config)
	float RetakeGroupRadius = 500.0f;

	UPROPERTY(Config)
	float RetakeWaitTime = 8.0f;

	/**
	 * A counter-terrorist gives the retake up and saves itself (CS's bots) when the terrorists alive outnumber its
	 * team by this many, or when it cannot reach and defuse the bomb in the time left; 0 never for the numbers.
	 */
	UPROPERTY(Config)
	int32 RetakeGiveUpAdvantage = 2;

	/** A defuse starts once no enemy was seen for this long (the site clear), s, unless the time runs out. */
	UPROPERTY(Config)
	float SiteClearTime = 2.5f;

	// Combat movement (ps2-shipping N30e)

	/** Seconds each way of the strafe while engaging, drawn from the bot's stream between these. */
	UPROPERTY(Config)
	float StrafeMinTime = 0.4f;

	UPROPERTY(Config)
	float StrafeMaxTime = 1.0f;

	/** The strafe at the walk key's speed (its spread is smaller than a run's). */
	UPROPERTY(Config)
	bool bStrafeWalking = true;

	/** An enemy this far or farther: a bot with a rifle (the AK-47, the M4A1) crouches and stops to fire, cm. */
	UPROPERTY(Config)
	float CrouchFireDistance = 1500.0f;

	// Grenades (ps2-shipping N30e)

	/** What the bot buys with the money left after the rest, in order, within CS's limits (Buy's names). */
	UPROPERTY(Config)
	TArray<FString> GrenadeBuyOrder;

	/** The chance a round that the bot throws its grenade on the way into the site, and a spot that it gets one. */
	UPROPERTY(Config)
	float GrenadeChance = 0.6f;

	/** How near the site (the planted bomb for a CT) the entry's grenade is thrown, cm. */
	UPROPERTY(Config)
	float EntryGrenadeDistance = 1800.0f;

	/** The nearest and farthest spot a grenade is thrown at, cm (nearer, the HE's 889 cm would hurt the thrower). */
	UPROPERTY(Config)
	float MinGrenadeDistance = 900.0f;

	UPROPERTY(Config)
	float MaxGrenadeDistance = 2200.0f;

	/** How far off the aimed point a throw may land, cm (drawn from the bot's stream). */
	UPROPERTY(Config)
	float GrenadeThrowError = 100.0f;

	/** Seconds after a throw before the next one is considered. */
	UPROPERTY(Config)
	float GrenadeCooldown = 4.0f;

	// Blind fire (ps2-shipping N30e)

	/** How far the blind fire strays from where the bot aims, degrees across (a quarter of it up and down). */
	UPROPERTY(Config)
	float BlindFireError = 25.0f;

	/** Seconds between the blind fire's new aim points, drawn between these; the chance it fires at a point. */
	UPROPERTY(Config)
	float BlindFireMinTime = 0.25f;

	UPROPERTY(Config)
	float BlindFireMaxTime = 0.6f;

	UPROPERTY(Config)
	float BlindFireChance = 0.6f;

	// The radio (ps2-shipping N30e)

	/** Seconds after a teammate's message in which the bot does not say it again. */
	UPROPERTY(Config)
	float RadioRepeatTime = 3.0f;

	/** How near a teammate's spotted enemy the bot must be to go and look, cm. */
	UPROPERTY(Config)
	float RadioReportRange = 3000.0f;

	/** The health below which the bot asks for backup (once a round). */
	UPROPERTY(Config)
	float NeedBackupHealth = 40.0f;

	/** The bot's pawn, as the game's class. */
	[[nodiscard]] AShooterCharacter* GetShooterPawn() const;
	[[nodiscard]] const UBlackboardComponent& GetBlackboard() const
	{
		return Tree.GetBlackboard();
	}
	/** The enemy engaged, or null. */
	[[nodiscard]] AShooterCharacter* GetEnemy() const;
	/**
	 * The name of the tree's branch that ran last (Idle, Blind, Engage, ThrowGrenade, Defuse, Plant, FetchBomb, PickUp,
	 * Escort, Investigate, Hunt, Objective).
	 */
	[[nodiscard]] FName GetCurrentTask() const
	{
		return CurrentTask;
	}
	/** The aim error now, degrees (after the time on target). */
	[[nodiscard]] float GetCurrentAimError() const;
	/** The part of each recoil kick pulled down: RecoilCompensation, within [0, 1]. */
	[[nodiscard]] float GetRecoilCompensation() const;

	/**
	 * Buys for the round as the tree's first branch does (money permitting), by its team's plan
	 * (AShooterGameMode::GetTeamBuyPlan): nothing on an eco unless it can afford the full buy; else a primary (the AWP
	 * now and then with the money for it and a helmet, the team's rifle, else an MP5 with kevlar, else a Desert Eagle
	 * with kevlar), the primary's ammunition until full, kevlar with a helmet (or kevlar), the kit for a CT, then the
	 * grenades of GrenadeBuyOrder it can afford. Returns what was bought.
	 */
	TArray<FString> BuyForRound();

	/** Seconds between a bot's looks (10 Hz). */
	static constexpr float SensingInterval = 0.1f;

	/**
	 * Spreads the bots' looks (the game mode gives each bot its slot as it joins): the bot in Slot of NumSlots looks
	 * Slot / NumSlots of an interval after slot 0, so ten bots look one or two a frame at 60 fps.
	 */
	void SetSensingSlot(int32 Slot, int32 NumSlots);

	/**
	 * The bot's index among the game mode's bots, in the order they were created (AShooterGameMode::AddBots, before
	 * the bot first takes a pawn): with the game mode's RandomSeed it seeds the bot's stream, so a bot's name never
	 * changes the match. -1 for a controller the game mode did not create (the tests').
	 */
	void SetBotIndex(int32 InBotIndex)
	{
		BotIndex = InBotIndex;
	}
	[[nodiscard]] int32 GetBotIndex() const
	{
		return BotIndex;
	}

	/** The bot's senses. */
	[[nodiscard]] UShooterPawnSensingComponent* GetPawnSensing() const
	{
		return PawnSensing;
	}

	// Grenades

	/**
	 * Starts a throw of Grenade (one of the pawn's) at TargetLocation: the ThrowGrenade branch draws it, turns and
	 * throws. The aim point is off by up to GrenadeThrowError (the bot's stream). False when the bot cannot throw now.
	 */
	bool ThrowGrenadeAt(AShooterWeapon_Projectile* Grenade, const FVector& TargetLocation);
	/** A throw is under way (drawing, aiming, or turned away from its flashbang). */
	[[nodiscard]] bool IsThrowingGrenade() const
	{
		return bThrowing;
	}
	/** Where the last throw was aimed (after the error). */
	[[nodiscard]] const FVector& GetThrowTarget() const
	{
		return ThrowTarget;
	}
	/**
	 * The pitch, degrees, of a throw at Speed (cm/s) under Gravity (cm/s^2, positive) that comes down Horizontal cm
	 * away and Height cm higher: the low arc, or 45 degrees when out of reach.
	 */
	[[nodiscard]] static float ComputeThrowPitch(float Horizontal, float Height, float Speed, float Gravity);

	// Combat movement

	/** The strafe's side while engaging: 1 to the right, -1 to the left, 0 none (standing, crouched, the AWP). */
	[[nodiscard]] float GetStrafeDirection() const
	{
		return bStrafingThisTick ? StrafeDirection : 0.0f;
	}

	// No enemy in sight

	/** The weapon on the floor the bot goes for (the PickUp branch), or null. */
	[[nodiscard]] AShooterWeapon* GetPickupTarget() const
	{
		return PickupTarget.Get();
	}
	/** The retake has gone in (the counter-terrorists gathered, waited long enough, or the time is short). */
	[[nodiscard]] bool IsRetaking() const
	{
		return bRetakeGo;
	}
	/** Where the counter-terrorists gather before the retake (valid once the bot went for it this round). */
	[[nodiscard]] const FVector& GetRetakeStaging() const
	{
		return RetakeStaging;
	}
	/** The lookouts visited this round (arrivals, the first included). */
	[[nodiscard]] int32 GetNumLookoutsVisited() const
	{
		return NumLookoutsVisited;
	}
	/** The site whose lookouts the bot keeps (NAME_None before the objective's first tick). */
	[[nodiscard]] FName GetLookoutSite() const
	{
		return LookoutSite;
	}

	// The radio

	/** A teammate's radio message (AShooterGameMode::SendRadioMessage): a spotted enemy near it is a place to look. */
	void OnRadioMessage(const FShooterRadioEntry& Entry);
	/**
	 * The bot is the one to answer a teammate's request: "Reporting in." to "Report in, team.", else "Affirmative.";
	 * for "Need backup." and "Taking fire" it goes to the sender.
	 */
	void AnswerRadioRequest(const FShooterRadioEntry& Entry);

	void Tick(float DeltaSeconds) override;

protected:
	/** Seeds the bot's stream once and starts over. */
	void OnPossess(APawn* InPawn) override;
	void OnUnPossess() override;

private:
	/** The difficulty applied (ApplyDifficulty). */
	EShooterBotDifficulty Difficulty = EShooterBotDifficulty::Normal;

	/** Builds the tree's nodes (once). */
	void BuildTree();
	/** Refreshes the blackboard from the senses and the game (the tree's decorators read it). */
	void UpdateBlackboard();
	void OnSeePawn(APawn* SeenPawn);
	void OnHearNoise(APawn* NoiseInstigator, const FVector& Location, float Volume);

	// The tree's tasks
	EBTNodeResult TaskIdle();
	EBTNodeResult TaskBlind(float DeltaTime);
	EBTNodeResult TaskEngage(float DeltaTime);
	EBTNodeResult TaskThrowGrenade(float DeltaTime);
	EBTNodeResult TaskDefuse(float DeltaTime);
	EBTNodeResult TaskPlant();
	EBTNodeResult TaskFetchBomb();
	EBTNodeResult TaskPickUp();
	EBTNodeResult TaskEscort(float DeltaTime);
	EBTNodeResult TaskInvestigate();
	EBTNodeResult TaskHunt(float DeltaTime);
	EBTNodeResult TaskObjective(float DeltaTime);
	/** A terrorist with the bomb planted holds near it (TaskObjective's post-plant; see the class comment). */
	EBTNodeResult HoldPlantedBomb(const AShooterBomb& Bomb, const TArray<FShooterLookout>& Lookouts, float DeltaTime);

	/** Moves to Goal unless already moving there (a new path only when the goal moves). */
	void MoveToGoal(const FVector& Goal);
	/** Stands still: no path, no wish. */
	void StandStill();
	/** A counter-terrorist gives the planted bomb up (RetakeGiveUpAdvantage, or no time to defuse it). */
	[[nodiscard]] bool ShouldGiveUpRetake(const AShooterBomb& Bomb) const;
	/** A terrorist with the bomb planted: it holds near it (PostPlantHoldRadius). */
	[[nodiscard]] bool IsHoldingPlant() const;
	/** The retake's gathering point for Bomb (the waypoint nearest to RetakeStagingDistance toward the CT spawn). */
	[[nodiscard]] FVector FindRetakeStaging(const FVector& BombLocation) const;
	/** The reaction time (with the difficulty) has passed since the enemy came into sight: the bot may fire. */
	[[nodiscard]] bool HasReacted() const;
	/** The knife's fight (Engage with the knife drawn; see the class comment). */
	EBTNodeResult EngageWithKnife(AShooterWeapon_Knife& Knife, float DeltaTime);
	/** Chooses the weapon on the floor to go for, a few times a second (the PickUp branch's). */
	void UpdatePickupTarget();
	/** Turns the view toward Wanted at Rate degrees a second (no weapon's kick: looking, not aiming). */
	void LookToward(const FRotator& Wanted, float Rate, float DeltaTime);
	/**
	 * Stands still and watches Yaws (a lookout's): the first (the main way in) for twice a drawn time between each of
	 * the others in turn (from a drawn one), each a drawn time; again from the first when Yaws change.
	 */
	void Watch(const TArray<float, TInlineAllocator<4>>& Yaws, float DeltaTime);
	/** On a ladder: faces it and climbs toward the path's point, or jumps off it at the bottom. */
	void UpdateLadderClimb(AShooterCharacter& Self);
	/** The trigger up. */
	void ReleaseTrigger();
	/**
	 * Aims Weapon at Wanted: the bot's own aim (the control rotation without the weapon's recoil kick) turns toward it
	 * at the turn rate, after pulling down GetRecoilCompensation of the kick added since the last tick; the kick left
	 * rides on top. Returns the angle left between the bot's own aim and Wanted, degrees.
	 */
	float AimToward(const FRotator& Wanted, AShooterWeapon& Weapon, float DeltaTime);
	/** The strafe across the line to EnemyFeet this tick (its side changes at its time, from the stream). */
	void UpdateStrafe(const FVector& EnemyFeet);
	/** Stands up from a crouch the engagement took. */
	void EndCombatCrouch();
	/** Decides whether to start a throw now (see the class comment). */
	void ConsiderGrenade();
	/** The pawn's grenade of Class, or null. */
	[[nodiscard]] AShooterWeapon_Projectile* FindGrenade(const UClass* GrenadeClass) const;
	/** The throw is over or given up. */
	void EndThrow();
	/**
	 * Radios Message (AShooterGameMode::SendRadioMessage) unless a teammate said it within RadioRepeatTime; true when
	 * sent.
	 */
	bool SayOnRadio(EShooterRadioMessage Message, const FVector* Location = nullptr);
	/** The bot goes to look at Location (the blackboard's noise, as a heard shot). */
	void LookAt(const FVector& Location);
	/** The bot's index in its team (the player states' order), for the CT's site split and rotation. */
	[[nodiscard]] int32 GetTeamIndex() const;
	[[nodiscard]] AShooterGameMode* GetShooterGameMode() const;
	[[nodiscard]] float GetWorldTime() const;

	UPROPERTY()
	UShooterPawnSensingComponent* PawnSensing = nullptr;

	/** The enemy engaged (a weak memory: the blackboard's Enemy). */
	UPROPERTY(Transient)
	AShooterCharacter* Enemy = nullptr;

	/** The behavior: its nodes are owned here, the tree holds the root. */
	UBehaviorTree Tree;
	TArray<TUniquePtr<UBTNode>> TreeNodes;

	FRandomStream BotRandom;
	bool bRandomSeeded = false;
	/** SetBotIndex's index (the stream's seed with the game mode's). */
	int32 BotIndex = -1;
	FName CurrentTask;
	/** The round serial (AShooterGameState::GetRoundSerial) of the last purchase, and of the round being played. */
	int32 BoughtInRound = -1;
	int32 ObservedRoundSerial = -1;

	/** The engagement: when and where the enemy was last seen, when first seen, the aim's offset, the burst. */
	float EnemyLastSeenTime = -1.0f;
	FVector EnemyLastSeenLocation = FVector::ZeroVector;
	float EnemyFirstSeenTime = -1.0f;
	FRotator AimOffset = FRotator::ZeroRotator;
	int32 BurstShotsFired = 0;
	float BurstPauseEndTime = 0.0f;
	int32 ShotsAtBurstStart = 0;
	bool bTriggerHeld = false;

	/**
	 * The recoil control: the weapon aimed last tick and its kick then (degrees up), and whether the bot aimed in the
	 * last tick and in this one (a new engagement starts from the kick as it is).
	 */
	TWeakObjectPtr<AShooterWeapon> RecoilWeapon;
	float LastRecoilKick = 0.0f;
	bool bAimedLastTick = false;
	bool bAimedThisTick = false;

	/** The strafe: its side (0 until the engagement draws one), when it turns, this tick's wish. */
	float StrafeDirection = 0.0f;
	float NextStrafeChangeTime = 0.0f;
	FVector StrafeWish = FVector::ZeroVector;
	bool bStrafingThisTick = false;
	bool bStrafedLastTick = false;
	/** The engagement crouched the pawn (a rifle at range). */
	bool bCombatCrouch = false;

	/** The blind fire: where it aims, whether it fires there, until when. */
	FRotator BlindAim = FRotator::ZeroRotator;
	float NextBlindAimTime = 0.0f;
	bool bBlindFiring = false;

	/** The throw under way: its grenade, its aim, when it began; after a flashbang, turned away until TurnAwayEndTime.
	 */
	TWeakObjectPtr<AShooterWeapon_Projectile> ThrowingGrenade;
	FVector ThrowTarget = FVector::ZeroVector;
	FRotator ThrowRotation = FRotator::ZeroRotator;
	float ThrowStartTime = 0.0f;
	float TurnAwayEndTime = -1.0f;
	bool bThrowing = false;
	bool bThrown = false;
	/** When a throw may next be considered, and the next check. */
	float NextGrenadeTime = 0.0f;
	float NextGrenadeCheckTime = 0.0f;
	/** This round: the entry's grenade drawn and thrown; the last enemy spot a grenade was drawn for. */
	bool bUsesEntryGrenade = false;
	bool bEntryGrenadeDone = false;
	FVector LastGrenadeSpot = FVector(1.0e9f, 1.0e9f, 1.0e9f);

	/** The radio this round: backup asked for. */
	bool bAskedForBackup = false;

	/** The last goal MoveToGoal was given. */
	FVector CurrentGoal = FVector::ZeroVector;
	bool bHasGoal = false;
	float NoiseHeardTime = -1.0f;

	/** The CT's rotation: sites moved on this round, and since when it holds its site (-1: not there). */
	int32 SiteRotation = 0;
	float HoldingSinceTime = -1.0f;

	/**
	 * The lookouts: the site whose lookouts the bot keeps, the one it goes to or stands at, when it leaves it (-1: not
	 * there yet), and how many it reached this round.
	 */
	FName LookoutSite;
	int32 LookoutIndex = 0;
	float LookoutLeaveTime = -1.0f;
	int32 NumLookoutsVisited = 0;

	/** The watch (Watch): the directions, the one watched, the last of the others watched, when it turns. */
	TArray<float, TInlineAllocator<4>> WatchYaws;
	int32 WatchIndex = 0;
	int32 WatchOther = 0;
	float NextWatchTime = 0.0f;
	/** The bot turned its view this tick (a task looked or aimed): the path does not turn it. */
	bool bLookedThisTick = false;
	/** The knife's rush this tick: its moves are run, not walked. */
	bool bRushingThisTick = false;
	/** The walk key is held for the strafe. */
	bool bWalkingForStrafe = false;

	/** The post-plant hold (a terrorist): holding since the plant, and the radio's call made. */
	bool bHoldingPlant = false;
	bool bHoldCalled = false;
	/** The retake (a counter-terrorist): its gathering point, whether it went in, since when it waits there. */
	FVector RetakeStaging = FVector::ZeroVector;
	bool bHasRetakeStaging = false;
	bool bRetakeGo = false;
	float RetakeWaitStart = -1.0f;
	/** The retake given up this round (ShouldGiveUpRetake): the bot saves itself until the round's end. */
	bool bGaveUpRetake = false;

	/** The hunt's next waypoint (a draw of the graph's) once at the enemy's spawn. */
	FVector HuntGoal = FVector::ZeroVector;
	bool bHasHuntGoal = false;

	/** The weapon on the floor it goes for, when it chose it, and the next choice. */
	TWeakObjectPtr<AShooterWeapon> PickupTarget;
	float PickupChosenTime = 0.0f;
	float NextPickupCheckTime = 0.0f;
	/** A weapon it went for and could not take this round (another took the spot, the walk failed). */
	TWeakObjectPtr<AShooterWeapon> GivenUpPickup;
};

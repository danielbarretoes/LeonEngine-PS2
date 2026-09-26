#pragma once

#include "AIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "ShooterAIController.generated.h"

class AShooterCharacter;
class AShooterGameMode;
class UPawnSensingComponent;

/**
 * A bot's controller (UE ShooterGame: AShooterAIController, whose bots run a behavior tree asset): Counter-Strike's
 * bot on Leon's behavior tree (a C++ tree over a typed blackboard), senses (UPawnSensingComponent: sight, and the
 * shots it hears) and the waypoint navigation (AAIController's MoveTo*).
 *
 * The tree, most urgent first (each tick):
 * 1. Frozen (the freeze time), dead, or stopped (bot_stop): stand; in the freeze, buy once (a rifle, the AWP now and
 *    then, armor, a kit).
 * 2. An enemy in sight: engage (Engage). Stop, turn to it at AimTurnRate, and fire once ReactionTime has passed since
 *    it came into sight: the aim is off by an error that starts at AimError and shrinks with the time on target
 *    (AimErrorDecayTime), drawn again for each burst; automatic weapons fire bursts of BurstShots with BurstPause
 *    between; the AWP zooms first. An enemy out of sight for EnemyMemory seconds is forgotten (its last place is
 *    searched).
 * 3. A counter-terrorist and the bomb planted: go to it and defuse (hold the use key).
 * 4. The bomb's carrier: go to the round's site (AShooterGameMode::GetTerroristTargetSite) and plant once inside.
 * 5. A terrorist and the bomb dropped: fetch it.
 * 6. A shot heard (an enemy's): go and look where it came from.
 * 7. The objective: the terrorists go to the round's site (and guard the bomb once planted); the counter-terrorists
 *    hold a site each (A for the even ones, B for the odd, by their place in the team), and retake the planted bomb.
 *
 * Difficulty (config, [/Script/ShooterGame.ShooterAIController]): Difficulty scales the reaction time and the aim
 * error down and the turn rate up (1: the values as set). Every random choice (the aim error, the AWP) comes from the
 * bot's stream, seeded from the game mode's RandomSeed and the bot's name, so a match with a seed replays.
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

	/** Scales the skill (see the class comment): 0.5 easy, 1 normal, 2 hard. */
	UPROPERTY(Config)
	float Difficulty = 1.0f;

	/** Seconds from an enemy coming into sight to the first shot. */
	UPROPERTY(Config)
	float ReactionTime = 0.35f;

	/** The aim's error on a new target, degrees, and how fast it settles (seconds for 1/e) down to MinAimError. */
	UPROPERTY(Config)
	float AimError = 5.0f;

	UPROPERTY(Config)
	float AimErrorDecayTime = 0.8f;

	UPROPERTY(Config)
	float MinAimError = 0.4f;

	/** How fast the bot turns to its aim, degrees a second. */
	UPROPERTY(Config)
	float AimTurnRate = 360.0f;

	/** Shots a burst of an automatic weapon, and the pause after it, seconds. */
	UPROPERTY(Config)
	int32 BurstShots = 4;

	UPROPERTY(Config)
	float BurstPause = 0.35f;

	/** Seconds an enemy out of sight is remembered. */
	UPROPERTY(Config)
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

	/** The bot's pawn, as the game's class. */
	[[nodiscard]] AShooterCharacter* GetShooterPawn() const;
	[[nodiscard]] const UBlackboardComponent& GetBlackboard() const
	{
		return Tree.GetBlackboard();
	}
	/** The enemy engaged, or null. */
	[[nodiscard]] AShooterCharacter* GetEnemy() const;
	/**
	 * The name of the tree's branch that ran last (Idle, Engage, Defuse, Plant, FetchBomb, Escort, Investigate, Hunt,
	 * Objective).
	 */
	[[nodiscard]] FName GetCurrentTask() const
	{
		return CurrentTask;
	}
	/** The aim error now, degrees (after the difficulty and the time on target). */
	[[nodiscard]] float GetCurrentAimError() const;

	/** Buys for the round as the tree's first branch does (money permitting); returns what was bought. */
	TArray<FString> BuyForRound();

	void Tick(float DeltaSeconds) override;

protected:
	void OnPossess(APawn* InPawn) override;
	void OnUnPossess() override;

private:
	/** Builds the tree's nodes (once). */
	void BuildTree();
	/** Refreshes the blackboard from the senses and the game (the tree's decorators read it). */
	void UpdateBlackboard();
	void OnSeePawn(APawn* SeenPawn);
	void OnHearNoise(APawn* NoiseInstigator, const FVector& Location, float Volume);

	// The tree's tasks
	EBTNodeResult TaskIdle();
	EBTNodeResult TaskEngage(float DeltaTime);
	EBTNodeResult TaskDefuse();
	EBTNodeResult TaskPlant();
	EBTNodeResult TaskFetchBomb();
	EBTNodeResult TaskEscort(float DeltaTime);
	EBTNodeResult TaskInvestigate();
	EBTNodeResult TaskHunt(float DeltaTime);
	EBTNodeResult TaskObjective(float DeltaTime);

	/** Moves to Goal unless already moving there (a new path only when the goal moves). */
	void MoveToGoal(const FVector& Goal);
	/** Stands still: no path, no wish. */
	void StandStill();
	/** Stands still at a goal, looking around slowly. */
	void HoldAndLookAround(float DeltaTime);
	/** The trigger up. */
	void ReleaseTrigger();
	/** Turns the control rotation toward AimTarget at the turn rate; the angle left, degrees. */
	float TurnToward(const FVector& AimTarget, float DeltaTime);
	/** The bot's index in its team (the player states' order), for the CT's site split and rotation. */
	[[nodiscard]] int32 GetTeamIndex() const;
	[[nodiscard]] AShooterGameMode* GetShooterGameMode() const;
	[[nodiscard]] float GetWorldTime() const;

	UPROPERTY()
	UPawnSensingComponent* PawnSensing = nullptr;

	/** The enemy engaged (a weak memory: the blackboard's Enemy). */
	UPROPERTY(Transient)
	AShooterCharacter* Enemy = nullptr;

	/** The behavior: its nodes are owned here, the tree holds the root. */
	UBehaviorTree Tree;
	TArray<TUniquePtr<UBTNode>> TreeNodes;

	FRandomStream BotRandom;
	bool bRandomSeeded = false;
	FName CurrentTask;
	/** The round serial (AShooterGameState::GetRoundSerial) of the last purchase, and of the round being played. */
	int32 BoughtInRound = -1;
	int32 ObservedRoundSerial = -1;

	/** The engagement: when the enemy was last seen and first seen, the aim's offset, the burst. */
	float EnemyLastSeenTime = -1.0f;
	float EnemyFirstSeenTime = -1.0f;
	FRotator AimOffset = FRotator::ZeroRotator;
	int32 BurstShotsFired = 0;
	float BurstPauseEndTime = 0.0f;
	int32 ShotsAtBurstStart = 0;
	bool bTriggerHeld = false;

	/** The last goal MoveToGoal was given. */
	FVector CurrentGoal = FVector::ZeroVector;
	bool bHasGoal = false;
	float NoiseHeardTime = -1.0f;

	/** The CT's rotation: sites moved on this round, and since when it holds its site (-1: not there). */
	int32 SiteRotation = 0;
	float HoldingSinceTime = -1.0f;
};

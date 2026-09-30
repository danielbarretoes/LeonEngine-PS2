#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/CharacterAnimInstance.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/DamageEvents.h"
#include "Engine/TriggerVolume.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "ShooterAIController.h"
#include "ShooterCharacter.h"
#include "ShooterCharacterMovement.h"
#include "ShooterGameMode.h"
#include "ShooterGameState.h"
#include "ShooterPawnSensingComponent.h"
#include "ShooterPlayerState.h"
#include "Tests/ScopedTestWorld.h"
#include "Tests/SkinnedTestMesh.h"

#if WITH_DEV_AUTOMATION_TESTS

// ps2-shipping N30c: CS 1.6's movement (UShooterCharacterMovement's class comment): fall damage, ladders, the jump's
// stamina, tagging and the footsteps. Every test steps the world at the fixed 30 Hz step (D4), so each replays the
// same numbers.

namespace
{

	constexpr float Step = 1.0f / 30.0f;

	void TickSteps(UWorld& World, int32 Steps)
	{
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			World.Tick(Step);
		}
	}

	/** A floor 80 m square, its top at Z = 0. */
	void SpawnFloor(UWorld& World)
	{
		(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
			FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -50.0f), FVector(80.0f, 80.0f, 1.0f)));
	}

	/** The game mode of the tests: its registries (the ladders) and its kill feed, no bots of its own. */
	AShooterGameMode* SetUpGameMode(UWorld& World)
	{
		AShooterGameMode* GameMode = Cast<AShooterGameMode>(World.SetGameMode(AShooterGameMode::StaticClass()));
		GameMode->bFillTeamsWithBots = false;
		return GameMode;
	}

	/** A character at Feet facing Yaw, possessed by a brainless bot of Team: the test drives it. */
	AShooterCharacter* SpawnShooter(UWorld& World, const FVector& Feet, float Yaw, EShooterTeam Team)
	{
		AShooterCharacter* Character = World.SpawnActor<AShooterCharacter>(Feet, FRotator(0.0f, Yaw, 0.0f));
		AShooterAIController* Controller = World.SpawnActor<AShooterAIController>();
		Controller->SetActorTickEnabled(false);
		if (AShooterPlayerState* State = Controller->GetPlayerState<AShooterPlayerState>())
		{
			State->SetTeam(Team);
		}
		Controller->Possess(Character);
		Controller->SetControlRotation(FRotator(0.0f, Yaw, 0.0f));
		return Character;
	}

	/** Steps with the stick held along Direction, as the player's MoveForward / MoveRight add it. */
	void MoveSteps(UWorld& World, AShooterCharacter& Character, const FVector& Direction, int32 Steps)
	{
		for (int32 Index = 0; Index < Steps; ++Index)
		{
			// APawn's input vector (ACharacter's one-argument AddMovementInput is Leon's wish setter).
			Character.APawn::AddMovementInput(Direction, 1.0f);
			World.Tick(Step);
		}
	}

	/** The world's deaths in the kill feed: no killer, the world's weapon. */
	int32 CountWorldKills(const AShooterGameMode& GameMode)
	{
		int32 Count = 0;
		for (const FShooterKillFeedEntry& Entry : GameMode.GetShooterGameState()->GetKillFeed())
		{
			Count += Entry.KillerName.IsEmpty() && Entry.WeaponName == TEXT("world") ? 1 : 0;
		}
		return Count;
	}

	/**
	 * Drops Character from where it stands to the floor; returns the speed it hit the floor at (the last step's
	 * falling speed plus that step's gravity), 0 when it did not land.
	 */
	float DropToTheFloor(UWorld& World, AShooterCharacter& Character)
	{
		const UShooterCharacterMovement& Move = *Character.GetShooterCharacterMovement();
		float LastSpeed = 0.0f;
		for (int32 Index = 0; Index < 300; ++Index)
		{
			World.Tick(Step);
			if (Character.IsMovingOnGround() || !Character.IsAlive())
			{
				return LastSpeed + (Move.Gravity * Step);
			}
			LastSpeed = -Character.GetVelocityZ();
		}
		return 0.0f;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMovementFallDamageTest, "ShooterGame.Movement.FallDamage",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMovementFallDamageTest::RunTest(const FString& Parameters)
{
	// CS 1.6's multiplayer FlPlayerFallDamage: no damage up to 580 units a second, then (speed - 580) x 100 / (1024 -
	// 580) x 1.25, lethal from about 935 u/s; a fatal fall is the world's kill. Drops of 3, 9 and 16 m at the fixed
	// step: safe, hurt (armor untouched, no tagging), dead (a 16 m drop left about 10 health without the 1.25).
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	AShooterGameMode* GameMode = SetUpGameMode(World);
	AShooterCharacter* Safe = SpawnShooter(World, FVector(0.0f, 0.0f, 300.0f), 0.0f, EShooterTeam::CT);
	const UShooterCharacterMovement& Move = *Safe->GetShooterCharacterMovement();

	// The thresholds, at 2.54 cm a unit.
	TestEqual("Safe: 580 u/s", Move.SafeFallSpeed, 580.0f * 2.54f, 0.1f);
	TestEqual("CS's 1024 u/s", Move.FatalFallSpeed, 1024.0f * 2.54f, 0.1f);
	TestEqual("The multiplayer scale", Move.FallDamageScale, 1.25f);
	TestEqual("A jump's landing is safe", Move.GetFallDamage(Move.JumpZVelocity), 0.0f);
	TestEqual("At the safe speed: none", Move.GetFallDamage(Move.SafeFallSpeed), 0.0f);
	TestEqual("Halfway to 1024 u/s: 62.5", Move.GetFallDamage(0.5f * (Move.SafeFallSpeed + Move.FatalFallSpeed)), 62.5f,
		1.0e-3f);
	TestEqual("At 1024 u/s: 125", Move.GetFallDamage(Move.FatalFallSpeed), 125.0f, 1.0e-3f);
	// (v - 580) x 100 / 444 x 1.25 = 100 at v = 580 + 355.2 u/s.
	TestEqual("Lethal at about 935 u/s", Move.GetFallDamage(935.2f * 2.54f), 100.0f, 0.1f);
	TestTrue("Not below it", Move.GetFallDamage(930.0f * 2.54f) < 100.0f);

	const float SafeSpeed = DropToTheFloor(World, *Safe);
	TestTrue("3 m: landed below the safe speed", SafeSpeed > 0.0f && SafeSpeed < Move.SafeFallSpeed);
	TestEqual("3 m: unhurt", Safe->GetHealth(), 100.0f);

	AShooterCharacter* Hurt = SpawnShooter(World, FVector(500.0f, 0.0f, 900.0f), 0.0f, EShooterTeam::CT);
	Hurt->SetArmor(100.0f, true);
	const float HurtSpeed = DropToTheFloor(World, *Hurt);
	TestTrue(
		"9 m: between the safe and the fatal speed", HurtSpeed > Move.SafeFallSpeed && HurtSpeed < Move.FatalFallSpeed);
	TestEqual("9 m: the damage of its landing speed", Hurt->GetHealth(), 100.0f - Move.GetFallDamage(HurtSpeed), 0.01f);
	TestTrue("9 m: alive", Hurt->IsAlive());
	TestEqual("Armor does not take a fall", Hurt->GetArmor(), 100.0f);
	TestEqual("A fall does not tag", Hurt->GetShooterCharacterMovement()->GetVelocityModifier(), 1.0f);
	TestEqual("A fall costs no stamina", Hurt->GetShooterCharacterMovement()->GetJumpStamina(), 0.0f);
	TestEqual("Nobody died yet", CountWorldKills(*GameMode), 0);

	AShooterCharacter* Dead = SpawnShooter(World, FVector(1000.0f, 0.0f, 1600.0f), 0.0f, EShooterTeam::CT);
	const float DeadSpeed = DropToTheFloor(World, *Dead);
	TestTrue("16 m: short of 1024 u/s but past 935", DeadSpeed < Move.FatalFallSpeed && DeadSpeed > 935.2f * 2.54f);
	TestFalse("16 m: dead", Dead->IsAlive());
	TestEqual("The kill feed: the world killed it", CountWorldKills(*GameMode), 1);

	// A jump on the floor lands safely.
	Safe->Jump();
	const float JumpSpeed = DropToTheFloor(World, *Safe);
	TestTrue("A jump lands", JumpSpeed > 0.0f);
	TestEqual("A jump does not hurt", Safe->GetHealth(), 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMovementLadderTest, "ShooterGame.Movement.Ladder",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMovementLadderTest::RunTest(const FString& Parameters)
{
	// A ladder on the face of a 4 m block (a trigger volume tagged Ladder, 20 cm thin): walking into it grabs it;
	// forward looking at it climbs at CS's 200 units a second (508 cm/s), looking up 45 degrees faster (CS), straight
	// down climbs down; no input holds the climber (no gravity); a jump pushes it off at 270 u/s and it falls; climbing
	// on over the top ends on the roof. The face looks toward the climber (the bots turn to it, ps2-polish P3).
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	// The block: X 100 to 500, Y -300 to 300, its roof at Z = 400.
	(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
		FTransform(FQuat::Identity, FVector(300.0f, 0.0f, 200.0f), FVector(4.0f, 6.0f, 4.0f)));
	AShooterGameMode* GameMode = SetUpGameMode(World);
	// The ladder: X 80 to 100 against the block's face, Y -50 to 50, the floor to the roof.
	ATriggerVolume* Ladder = World.SpawnActor<ATriggerVolume>(ATriggerVolume::StaticClass(),
		FTransform(FQuat::Identity, FVector(90.0f, 0.0f, 200.0f), FVector(0.2f, 1.0f, 4.0f)));
	Ladder->Tags.Add(AShooterGameMode::LadderTag);
	TickSteps(World, 1);
	TestTrue("The game mode registers the ladder", GameMode->GetZones().Contains(Ladder));

	AShooterCharacter* Climber = SpawnShooter(World, FVector(-200.0f, 0.0f, 0.0f), 0.0f, EShooterTeam::CT);
	UShooterCharacterMovement& Move = *Climber->GetShooterCharacterMovement();
	TestTrue("No face off a ladder", Move.GetLadderNormal().IsZero());
	AController& Controller = *Climber->GetController();
	TestEqual("CS's climbing speed: 200 u/s", Move.LadderClimbSpeed, 200.0f * 2.54f, 0.1f);

	// Walking at the ladder grabs it.
	int32 WalkSteps = 0;
	while (!Move.IsOnLadder() && WalkSteps < 60)
	{
		MoveSteps(World, *Climber, FVector(1.0f, 0.0f, 0.0f), 1);
		++WalkSteps;
	}
	if (!TestTrue("On the ladder", Move.IsOnLadder() && Move.GetLadder() == Ladder))
	{
		return false;
	}
	TestTrue("Its face toward the climber", Move.GetLadderNormal().Equals(FVector(-1.0f, 0.0f, 0.0f)));
	TestTrue("A custom mode, neither walking nor falling",
		Climber->GetMovementMode() == EMovementMode::Custom && !Climber->IsFalling() && !Climber->IsMovingOnGround());

	// Forward, looking at the ladder: up at 508 cm/s.
	MoveSteps(World, *Climber, FVector(1.0f, 0.0f, 0.0f), 1);
	float Z = Climber->GetActorLocation().Z;
	MoveSteps(World, *Climber, FVector(1.0f, 0.0f, 0.0f), 9);
	TestEqual("Climbs at 508 cm/s", (Climber->GetActorLocation().Z - Z) / (9.0f * Step), 508.0f, 0.5f);
	TestEqual("Straight up the face", Climber->GetCharacterMovement().Velocity.Size2D(), 0.0f, 1.0e-3f);

	// Looking up 45 degrees: CS adds the view's rise to the climb, 200 x (sin 45 + cos 45) u/s.
	Controller.SetControlRotation(FRotator(45.0f, 0.0f, 0.0f));
	Z = Climber->GetActorLocation().Z;
	MoveSteps(World, *Climber, FVector(1.0f, 0.0f, 0.0f), 3);
	TestEqual("Faster looking up (CS)", (Climber->GetActorLocation().Z - Z) / (3.0f * Step), 508.0f * FMath::Sqrt(2.0f),
		1.0f);

	// No input: it hangs there, no gravity.
	Z = Climber->GetActorLocation().Z;
	TickSteps(World, 15);
	TestEqual("No gravity on a ladder", Climber->GetActorLocation().Z, Z, 1.0e-3f);
	TestTrue("Still on it", Move.IsOnLadder());

	// Looking straight down: forward climbs down.
	Controller.SetControlRotation(FRotator(-90.0f, 0.0f, 0.0f));
	Z = Climber->GetActorLocation().Z;
	MoveSteps(World, *Climber, FVector(1.0f, 0.0f, 0.0f), 6);
	TestEqual("Climbs down at 508 cm/s", (Z - Climber->GetActorLocation().Z) / (6.0f * Step), 508.0f, 0.5f);

	// A jump pushes it off the ladder: it falls away from the face and lands on the floor.
	const float JumpZ = Climber->GetActorLocation().Z;
	Climber->Jump();
	TickSteps(World, 1);
	TestFalse("Off the ladder", Move.IsOnLadder());
	TestTrue("Falling", Climber->IsFalling());
	TestEqual("Pushed off at 270 u/s", Climber->GetCharacterMovement().Velocity.X, -686.0f, 0.5f);
	TickSteps(World, 1);
	TestTrue("Gravity again", Climber->GetVelocityZ() < 0.0f && Climber->GetActorLocation().Z < JumpZ);
	for (int32 Index = 0; Index < 60 && !Climber->IsMovingOnGround(); ++Index)
	{
		TickSteps(World, 1);
	}
	TestTrue("On the floor, away from the ladder",
		Climber->IsMovingOnGround() && Climber->GetActorLocation().Z < 1.0f && Climber->GetActorLocation().X < 0.0f);
	TestTrue("Not grabbed again", !Move.IsOnLadder() && Climber->IsAlive());

	// Back to it, and climbing on over the top onto the roof.
	Controller.SetControlRotation(FRotator(0.0f, 0.0f, 0.0f));
	bool bLeftAtTheTop = false;
	for (int32 Index = 0; Index < 150; ++Index)
	{
		const bool bWasOnLadder = Move.IsOnLadder();
		MoveSteps(World, *Climber, FVector(1.0f, 0.0f, 0.0f), 1);
		bLeftAtTheTop |= bWasOnLadder && !Move.IsOnLadder() && Climber->GetActorLocation().Z >= 400.0f;
		if (Climber->IsMovingOnGround() && Climber->GetActorLocation().Z > 300.0f)
		{
			break;
		}
	}
	TestTrue("Left the ladder at its top", bLeftAtTheTop);
	TestTrue("Walking on the roof",
		Climber->IsMovingOnGround() && FMath::IsNearlyEqual(Climber->GetActorLocation().Z, 400.0f, 1.0f) &&
			Climber->GetActorLocation().X > 100.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMovementJumpStaminaTest, "ShooterGame.Movement.JumpStamina",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMovementJumpStaminaTest::RunTest(const FString& Parameters)
{
	// CS's fuser2: a jump costs 1.3158 s of stamina; on the floor each 10 ms scales the horizontal velocity by
	// 1 - stamina x 0.19 (0.75 at the jump), so a running jump's landing loses most of its speed, and the speed comes
	// back when the stamina has run out. The curve, step by step, is the formula's.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	(void)SetUpGameMode(World);
	AShooterCharacter* Runner = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	UShooterCharacterMovement& Move = *Runner->GetShooterCharacterMovement();
	TestEqual("A full stamina's ratio: 0.75", Move.GetJumpStaminaRatio(Move.JumpStaminaTime), 0.75f, 1.0e-3f);
	TestEqual("None: 1", Move.GetJumpStaminaRatio(0.0f), 1.0f);

	const FVector Forward(1.0f, 0.0f, 0.0f);
	MoveSteps(World, *Runner, Forward, 30);
	const float RunSpeed = Move.GetMaxSpeed();
	TestEqual("Running", Move.Velocity.Size2D(), RunSpeed, 0.01f);

	Runner->Jump();
	MoveSteps(World, *Runner, Forward, 1);
	TestTrue("In the air", Runner->IsFalling());
	TestEqual("The jump's stamina", Move.GetJumpStamina(), Move.JumpStaminaTime, 1.0e-5f);
	int32 AirSteps = 1;
	while (Runner->IsFalling() && AirSteps < 60)
	{
		MoveSteps(World, *Runner, Forward, 1);
		++AirSteps;
	}
	TestTrue("Landed", Runner->IsMovingOnGround());
	TestEqual("The air keeps the speed", Move.Velocity.Size2D(), RunSpeed, 0.01f);

	// On the floor: v' = min(run, v x ratio^(step / 10 ms) + acceleration x step), the stamina after the step's.
	float Expected = Move.Velocity.Size2D();
	float Slowest = Expected;
	bool bFollowsTheFormula = true;
	int32 GroundSteps = 0;
	while (Move.GetJumpStamina() > 0.0f && GroundSteps < 60)
	{
		MoveSteps(World, *Runner, Forward, 1);
		++GroundSteps;
		const float Scale = FMath::Pow(Move.GetJumpStaminaRatio(Move.GetJumpStamina()), Step / 0.01f);
		Expected = FMath::Min(RunSpeed, (Expected * Scale) + (Move.MaxAcceleration * Step));
		bFollowsTheFormula &= FMath::IsNearlyEqual(Move.Velocity.Size2D(), Expected, 0.5f);
		Slowest = FMath::Min(Slowest, Move.Velocity.Size2D());
	}
	TestTrue("The stamina's curve", bFollowsTheFormula);
	TestTrue("The landing loses a third of the speed", Slowest < 0.7f * RunSpeed);
	TestEqual("The stamina runs out 1.3 s after the jump's step", static_cast<float>(AirSteps - 1 + GroundSteps) * Step,
		Move.JumpStaminaTime, Step);
	MoveSteps(World, *Runner, Forward, 5);
	TestEqual("Full speed again", Move.Velocity.Size2D(), RunSpeed, 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMovementTaggingTest, "ShooterGame.Movement.Tagging",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMovementTaggingTest::RunTest(const FString& Parameters)
{
	// CS's m_flVelocityModifier: a shot halves the victim's speed, which comes back linearly in a second; a blow that
	// kills, or the world's damage, does not tag.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	(void)SetUpGameMode(World);
	AShooterCharacter* Victim = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	AShooterCharacter* Shooter = SpawnShooter(World, FVector(-2000.0f, 0.0f, 0.0f), 0.0f, EShooterTeam::T);
	UShooterCharacterMovement& Move = *Victim->GetShooterCharacterMovement();
	const FVector Forward(1.0f, 0.0f, 0.0f);
	MoveSteps(World, *Victim, Forward, 30);
	const float RunSpeed = Move.GetMaxSpeed();
	TestEqual("Running", Move.Velocity.Size2D(), RunSpeed, 0.01f);

	FHitResult Hit;
	Hit.bBlockingHit = true;
	Hit.ImpactPoint = Victim->GetActorLocation() + FVector(0.0f, 0.0f, 130.0f); // the chest: x1
	const FPointDamageEvent Shot(10.0f, Hit, Forward, nullptr);
	(void)Victim->TakeDamage(10.0f, Shot, Shooter->GetController(), nullptr);
	TestEqual("Hurt", Victim->GetHealth(), 90.0f);
	TestEqual("Tagged: half the speed", Move.GetVelocityModifier(), 0.5f);
	TestEqual("The velocity halves at once", Move.Velocity.Size2D(), 0.5f * RunSpeed, 0.01f);
	TestEqual("And so does the top speed", Move.GetMaxSpeed(), 0.5f * RunSpeed, 0.01f);

	MoveSteps(World, *Victim, Forward, 15);
	TestEqual("Half a second: three quarters", Move.GetVelocityModifier(), 0.75f, 1.0e-4f);
	TestTrue("Held to it", Move.Velocity.Size2D() <= (0.75f * RunSpeed) + 0.01f);
	MoveSteps(World, *Victim, Forward, 15);
	TestEqual("A second: recovered", Move.GetVelocityModifier(), 1.0f);
	MoveSteps(World, *Victim, Forward, 5);
	TestEqual("Full speed again", Move.Velocity.Size2D(), RunSpeed, 0.01f);

	// The world's damage (a fall, a pain volume) does not tag.
	(void)Victim->TakeDamage(10.0f, FDamageEvent(), nullptr, nullptr);
	TestEqual("The world's damage hurts", Victim->GetHealth(), 80.0f);
	TestEqual("Without tagging", Move.GetVelocityModifier(), 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FShooterMovementFootstepsTest, "ShooterGame.Movement.Footsteps",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FShooterMovementFootstepsTest::RunTest(const FString& Parameters)
{
	// The body's locomotion notifies are the footsteps: running, each Footstep_L / Footstep_R makes a noise an enemy
	// bot hears; walking (the walk key) and crouching are silent, although the notifies still come (CS: a step is
	// heard above 150 u/s). The test's locomotion has the footsteps at every speed.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	SpawnFloor(World);
	(void)SetUpGameMode(World);
	AShooterCharacter* Walker = SpawnShooter(World, FVector::ZeroVector, 0.0f, EShooterTeam::CT);
	AShooterCharacter* Listener = SpawnShooter(World, FVector(1500.0f, 800.0f, 0.0f), 180.0f, EShooterTeam::T);
	UShooterPawnSensingComponent* Sensing = Cast<AShooterAIController>(Listener->GetController())->GetPawnSensing();
	int32 Heard = 0;
	Sensing->OnHearNoise.AddLambda(
		[&Heard, Walker](APawn* Instigator, const FVector&, float) { Heard += Instigator == Walker ? 1 : 0; });

	USkeletalMesh* Body = FSkinnedTestCharacter::MakeMesh();
	if (!TestNotNull("The test body", Body))
	{
		return false;
	}
	Walker->SetSkeletalBody(Body);
	UAnimSequence* Steps =
		FSkinnedTestCharacter::MakeOffsetClip(FSkinnedTestCharacter::Root, FVector::ZeroVector, 1.0f);
	Steps->bLoop = true;
	Steps->AddNotify(TEXT("Footstep_L"), 0.25f);
	Steps->AddNotify(TEXT("Footstep_R"), 0.75f);
	UBlendSpace1D* Locomotion = NewObject<UBlendSpace1D>();
	(void)Locomotion->AddSample(Steps, 0.0f);
	UAnimInstance& Anim = Walker->GetMesh().GetAnimInstance();
	Anim.SetBlendSpace(Locomotion);
	// Crouched too (the game's crouched locomotion, N27's, is silent: this test's has the steps).
	if (UCharacterAnimInstance* CharacterAnim = Cast<UCharacterAnimInstance>(&Anim))
	{
		CharacterAnim->SetCrouchBlendSpace(Locomotion);
	}
	int32 Notifies = 0;
	Anim.OnAnimNotify.AddLambda([&Notifies](FName Name, const UAnimSequenceBase*)
		{ Notifies += Name == FName(TEXT("Footstep_L")) || Name == FName(TEXT("Footstep_R")) ? 1 : 0; });

	const FVector Forward(1.0f, 0.0f, 0.0f);
	MoveSteps(World, *Walker, Forward, 15);
	Notifies = 0;
	Heard = 0;
	MoveSteps(World, *Walker, Forward, 60);
	TestTrue("Running: the notifies come", Notifies >= 3);
	TestEqual("Every step is heard", Heard, Notifies);

	// The walk key: under 150 u/s, silent.
	Walker->SetWalking(true);
	MoveSteps(World, *Walker, Forward, 15);
	TestTrue("Walking speed", Walker->GetCharacterMovement().Velocity.Size2D() < 381.0f);
	Notifies = 0;
	Heard = 0;
	MoveSteps(World, *Walker, Forward, 60);
	TestTrue("Walking: the notifies come", Notifies >= 3);
	TestEqual("Walking: silent", Heard, 0);
	Walker->SetWalking(false);

	// Crouched: silent too.
	Walker->Crouch();
	MoveSteps(World, *Walker, Forward, 15);
	TestTrue("Crouched", Walker->bIsCrouched != 0);
	Notifies = 0;
	MoveSteps(World, *Walker, Forward, 60);
	TestTrue("Crouching: the notifies come", Notifies >= 3);
	TestEqual("Crouching: silent", Heard, 0);

	// Standing: the notifies of the idle, no steps.
	Walker->UnCrouch();
	TickSteps(World, 30);
	Notifies = 0;
	TickSteps(World, 60);
	TestTrue("Standing: the notifies come", Notifies >= 3);
	TestEqual("Standing: silent", Heard, 0);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

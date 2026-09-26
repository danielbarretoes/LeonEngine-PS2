#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "CoreMinimal.h"
#include "Engine/BlockingVolume.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerInput.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "Tests/EngineTestTypes.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

// UE's velocity model (UCharacterMovementComponent::bInstantVelocity false), crouching, the GetMaxSpeed hook, the
// first-person camera and the mouse sensitivity (P17). The instant model keeps its own tests and the goldens.

namespace
{

	constexpr float ModelTestDeltaTime = 1.0f / 60.0f;

	/** A walking character at the origin with UE's velocity model and a large walk area. */
	ACharacter& SpawnModelCharacter(UWorld& World, UClass* Class = nullptr)
	{
		ACharacter* Character = Class != nullptr ? World.SpawnActor<ACharacter>(Class) : World.SpawnActor<ACharacter>();
		UCharacterMovementComponent& Move = Character->GetCharacterMovement();
		Move.bInstantVelocity = false;
		Move.MaxWalkSpeed = 600.0f;
		Move.MaxAcceleration = 2048.0f;
		Move.GroundFriction = 8.0f;
		Move.BrakingDecelerationWalking = 2048.0f;
		Move.WalkBounds = 100000.0f;
		Character->Reset(FVector::ZeroVector);
		return *Character;
	}

	/** Moves the character Frames times at 60 Hz in Scene with a constant wish (zero for none). */
	void RunModelFrames(ACharacter& Character, FPhysScene& Scene, const FVector& Wish, int32 Frames)
	{
		for (int32 Frame = 0; Frame < Frames; ++Frame)
		{
			Character.AddMovementInput(Wish);
			Character.PerformMovement(Scene, ModelTestDeltaTime);
		}
	}

	/** A static box body (the crouch tests' ceiling). */
	void AddModelBox(FPhysScene& Scene, const FVector& Center, const FVector& HalfExtents)
	{
		const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
		Scene.GetBodies()[Id].Position = Center;
		Scene.GetBodies()[Id].HalfExtents = HalfExtents;
	}

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementAccelerationReachesMaxSpeedTest,
	"System.Engine.CharacterMovement.AccelerationReachesMaxSpeed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementAccelerationReachesMaxSpeedTest::RunTest(const FString& Parameters)
{
	// From rest a full input accelerates by MaxAcceleration (UE's CalcVelocity) until MaxWalkSpeed: 2048 cm/s^2 reach
	// 600 cm/s in the 18th frame at 60 Hz.
	FScopedTestWorld TestWorld;
	FPhysScene Scene;
	ACharacter& Character = SpawnModelCharacter(*TestWorld);
	const UCharacterMovementComponent& Move = Character.GetCharacterMovement();

	RunModelFrames(Character, Scene, FVector(1.0f, 0.0f, 0.0f), 1);
	TestEqual("One frame of acceleration", Move.Velocity.X, 2048.0f / 60.0f, 0.01f);
	TestEqual("Moved by it", Character.GetActorLocation().X, (2048.0f / 60.0f) / 60.0f, 1.0e-3f);
	TestEqual("The acceleration", Move.GetCurrentAcceleration().X, 2048.0f, 1.0e-3f);

	RunModelFrames(Character, Scene, FVector(1.0f, 0.0f, 0.0f), 16);
	TestTrue("Still accelerating after 17 frames", Move.Velocity.X < 599.0f);
	RunModelFrames(Character, Scene, FVector(1.0f, 0.0f, 0.0f), 1);
	TestEqual("MaxWalkSpeed in the 18th", Move.Velocity.X, 600.0f, 0.01f);
	RunModelFrames(Character, Scene, FVector(1.0f, 0.0f, 0.0f), 30);
	TestEqual("Never faster", Move.Velocity.X, 600.0f, 0.01f);
	TestTrue("Walking", Character.IsMovingOnGround());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementZeroStepKeepsTheVelocityTest,
	"System.Engine.CharacterMovement.ZeroStepKeepsTheVelocity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementZeroStepKeepsTheVelocityTest::RunTest(const FString& Parameters)
{
	// A step of 0 s (a paused or repeated frame) moves nothing and keeps the velocity: no 0 / 0 in the velocity the
	// move leaves (UE: MIN_TICK_TIME).
	FScopedTestWorld TestWorld;
	FPhysScene Scene;
	ACharacter& Character = SpawnModelCharacter(*TestWorld);
	const UCharacterMovementComponent& Move = Character.GetCharacterMovement();
	RunModelFrames(Character, Scene, FVector(1.0f, 0.0f, 0.0f), 10);
	const FVector Velocity = Move.Velocity;
	const FVector Location = Character.GetActorLocation();
	Character.AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
	Character.PerformMovement(Scene, 0.0f);
	TestFalse("No NaN", Move.Velocity.ContainsNaN());
	TestTrue("The velocity kept", Move.Velocity.Equals(Velocity, 1.0e-3f));
	TestTrue("Not moved", Character.GetActorLocation().Equals(Location, 1.0e-3f));
	RunModelFrames(Character, Scene, FVector(1.0f, 0.0f, 0.0f), 1);
	TestFalse("Still no NaN", Move.Velocity.ContainsNaN());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementBrakingStopsTheCharacterTest,
	"System.Engine.CharacterMovement.BrakingStopsTheCharacter",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementBrakingStopsTheCharacterTest::RunTest(const FString& Parameters)
{
	// Without an input the ground brakes (UE's ApplyVelocityBraking): the friction (GroundFriction x
	// BrakingFrictionFactor = 16) times the velocity plus BrakingDecelerationWalking, and it never reverses.
	FScopedTestWorld TestWorld;
	FPhysScene Scene;
	ACharacter& Character = SpawnModelCharacter(*TestWorld);
	const UCharacterMovementComponent& Move = Character.GetCharacterMovement();
	RunModelFrames(Character, Scene, FVector(1.0f, 0.0f, 0.0f), 30);

	RunModelFrames(Character, Scene, FVector::ZeroVector, 1);
	TestEqual("One braking step", Move.Velocity.X, 600.0f - ((16.0f * 600.0f) + 2048.0f) / 60.0f, 0.05f);

	float LastX = Character.GetActorLocation().X;
	int32 StopFrame = 0;
	for (int32 Frame = 2; Frame <= 20 && StopFrame == 0; ++Frame)
	{
		RunModelFrames(Character, Scene, FVector::ZeroVector, 1);
		TestTrue("Never backwards", Character.GetActorLocation().X >= LastX);
		LastX = Character.GetActorLocation().X;
		if (Move.Velocity.IsZero())
		{
			StopFrame = Frame;
		}
	}
	TestEqual("Stopped in the 6th frame", StopFrame, 6);

	// A separate braking friction replaces the ground friction (UE: bUseSeparateBrakingFriction).
	UCharacterMovementComponent& MutableMove = Character.GetCharacterMovement();
	MutableMove.bUseSeparateBrakingFriction = true;
	MutableMove.BrakingFriction = 0.0f;
	MutableMove.BrakingDecelerationWalking = 600.0f;
	RunModelFrames(Character, Scene, FVector(1.0f, 0.0f, 0.0f), 30);
	RunModelFrames(Character, Scene, FVector::ZeroVector, 1);
	TestEqual("Constant deceleration only", Move.Velocity.X, 600.0f - 10.0f, 0.05f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementGroundFrictionTurnsVelocityTest,
	"System.Engine.CharacterMovement.GroundFrictionTurnsVelocity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementGroundFrictionTurnsVelocityTest::RunTest(const FString& Parameters)
{
	// Changing direction: the friction turns the velocity toward the input (UE: V - (V - Dir * |V|) * dt * Friction),
	// then the input accelerates it.
	FScopedTestWorld TestWorld;
	FPhysScene Scene;
	ACharacter& Character = SpawnModelCharacter(*TestWorld);
	const UCharacterMovementComponent& Move = Character.GetCharacterMovement();
	RunModelFrames(Character, Scene, FVector(1.0f, 0.0f, 0.0f), 30);

	RunModelFrames(Character, Scene, FVector(0.0f, 1.0f, 0.0f), 1);
	const float Turn = 8.0f / 60.0f;
	TestEqual("X turned", Move.Velocity.X, 600.0f - 600.0f * Turn, 0.05f);
	TestEqual("Y turned and accelerated", Move.Velocity.Y, 600.0f * Turn + 2048.0f / 60.0f, 0.05f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementAirControlKeepsMomentumTest,
	"System.Engine.CharacterMovement.AirControlKeepsMomentum",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementAirControlKeepsMomentumTest::RunTest(const FString& Parameters)
{
	// In the air the velocity is kept (no friction, no braking by default) and the input only accelerates it by
	// AirControl x MaxAcceleration (UE's PhysFalling).
	FScopedTestWorld TestWorld;
	FPhysScene Scene;
	ACharacter& Character = SpawnModelCharacter(*TestWorld);
	UCharacterMovementComponent& Move = Character.GetCharacterMovement();
	Move.AirControl = 0.35f;
	RunModelFrames(Character, Scene, FVector(1.0f, 0.0f, 0.0f), 30);

	Character.Jump();
	RunModelFrames(Character, Scene, FVector(1.0f, 0.0f, 0.0f), 1);
	TestTrue("Jumped", Character.IsFalling());
	RunModelFrames(Character, Scene, FVector::ZeroVector, 10);
	TestTrue("Still in the air", Character.IsFalling());
	TestEqual("Momentum kept without input", Move.Velocity.X, 600.0f, 0.01f);

	RunModelFrames(Character, Scene, FVector(-1.0f, 0.0f, 0.0f), 1);
	TestEqual("Air control against it", Move.Velocity.X, 600.0f - (2048.0f * 0.35f) / 60.0f, 0.05f);

	// At low speed the air control is boosted (UE: AirControlBoostMultiplier below the threshold).
	Move.Velocity = FVector::ZeroVector;
	RunModelFrames(Character, Scene, FVector(1.0f, 0.0f, 0.0f), 1);
	if (Character.IsFalling())
	{
		TestEqual("Boosted air control", Move.Velocity.X, (2048.0f * 0.7f) / 60.0f, 0.05f);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementCrouchShrinksAndSlowsTest,
	"System.Engine.CharacterMovement.CrouchShrinksAndSlows",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementCrouchShrinksAndSlowsTest::RunTest(const FString& Parameters)
{
	// Crouching (NavAgentProps.bCanCrouch) takes CrouchedHalfHeight before the next move: the feet stay, the eyes come
	// down to CrouchedEyeHeight, the speed is MaxWalkSpeedCrouched and the capsule's body follows.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	FPhysScene Scene;
	ACharacter* Character = World.SpawnActor<ACharacter>(AEngineTestCharacter::StaticClass());
	Character->Reset(FVector::ZeroVector);
	const UCharacterMovementComponent& Move = Character->GetCharacterMovement();
	TestTrue("Can crouch", Character->CanCrouch());

	Character->Crouch();
	TestFalse("Not before the move", Character->bIsCrouched != 0);
	Character->PerformMovement(Scene, ModelTestDeltaTime);
	TestTrue("Crouched", Character->bIsCrouched != 0);
	TestEqual("Crouched half height", Character->GetCapsule().GetCapsuleHalfHeight(), Move.CrouchedHalfHeight);
	TestEqual("Feet stay", Character->GetActorLocation().Z, 0.0f, 1.0e-3f);
	TestEqual("Crouched eyes", Character->BaseEyeHeight, Character->CrouchedEyeHeight);
	TestEqual("Crouched speed", Move.GetMaxSpeed(), Move.MaxWalkSpeedCrouched);
	const int32 BodyIndex = World.GetPhysicsScene().FindComponentBody(*Character->GetCapsuleComponent());
	if (TestTrue("Capsule body", BodyIndex != INDEX_NONE))
	{
		TestEqual("The body shrank", World.GetPhysicsScene().GetBodies()[BodyIndex].HalfExtents.Z,
			Move.CrouchedHalfHeight, 1.0e-3f);
	}

	RunModelFrames(*Character, Scene, FVector(1.0f, 0.0f, 0.0f), 60);
	TestEqual(
		"Walked a second at the crouched speed", Character->GetActorLocation().X, Move.MaxWalkSpeedCrouched, 0.5f);

	Character->UnCrouch();
	RunModelFrames(*Character, Scene, FVector::ZeroVector, 1);
	TestFalse("Standing", Character->bIsCrouched != 0);
	TestEqual("Standing half height", Character->GetCapsule().GetCapsuleHalfHeight(), 92.5f);
	TestEqual("Standing eyes", Character->BaseEyeHeight, GetDefault<APawn>()->BaseEyeHeight);
	TestEqual("Walking speed", Move.GetMaxSpeed(), Move.MaxWalkSpeed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementUnCrouchNeedsRoomTest,
	"System.Engine.CharacterMovement.UnCrouchNeedsRoom",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementUnCrouchNeedsRoomTest::RunTest(const FString& Parameters)
{
	// Under a ceiling at 120 cm the standing capsule (185 cm) does not fit: the character stays crouched until it
	// walks out, then stands up by itself (bWantsToCrouch is off).
	FScopedTestWorld TestWorld;
	FPhysScene Scene;
	AddModelBox(Scene, FVector(0.0f, 0.0f, 170.0f), FVector(100.0f, 100.0f, 50.0f));
	ACharacter* Character = TestWorld->SpawnActor<ACharacter>(AEngineTestCharacter::StaticClass());
	Character->Reset(FVector::ZeroVector);
	Character->Crouch();
	Character->PerformMovement(Scene, ModelTestDeltaTime);
	TestTrue("Crouched", Character->bIsCrouched != 0);

	Character->UnCrouch();
	RunModelFrames(*Character, Scene, FVector::ZeroVector, 5);
	TestTrue("Blocked by the ceiling", Character->bIsCrouched != 0);
	TestEqual("Still the crouched capsule", Character->GetCapsule().GetCapsuleHalfHeight(),
		Character->GetCharacterMovement().CrouchedHalfHeight);

	RunModelFrames(*Character, Scene, FVector(1.0f, 0.0f, 0.0f), 60);
	TestTrue("Walked out", Character->GetActorLocation().X > 150.0f);
	TestFalse("Stood up in the open", Character->bIsCrouched != 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementCrouchNeedsTheAgentFlagTest,
	"System.Engine.CharacterMovement.CrouchNeedsTheAgentFlag",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementCrouchNeedsTheAgentFlagTest::RunTest(const FString& Parameters)
{
	// UE's default: a character cannot crouch until its NavAgentProps.bCanCrouch is on.
	FScopedTestWorld TestWorld;
	FPhysScene Scene;
	ACharacter* Character = TestWorld->SpawnActor<ACharacter>();
	Character->Reset(FVector::ZeroVector);
	TestFalse("Cannot crouch", Character->CanCrouch());
	Character->Crouch();
	Character->PerformMovement(Scene, ModelTestDeltaTime);
	TestFalse("Not crouched", Character->bIsCrouched != 0);
	TestEqual("Standing half height", Character->GetCapsule().GetCapsuleHalfHeight(), 92.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementCrouchInTheAirTucksTheFeetTest,
	"System.Engine.CharacterMovement.CrouchInTheAirTucksTheFeet",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementCrouchInTheAirTucksTheFeetTest::RunTest(const FString& Parameters)
{
	// In the air the capsule shrinks around its centre (UE): the feet come up by the half height it loses.
	FScopedTestWorld TestWorld;
	FPhysScene Scene;
	ACharacter* Character = TestWorld->SpawnActor<ACharacter>(AEngineTestCharacter::StaticClass());
	Character->Reset(FVector(0.0f, 0.0f, 500.0f));
	Character->PerformMovement(Scene, ModelTestDeltaTime);
	TestTrue("Falling", Character->IsFalling());

	const float Before = Character->GetActorLocation().Z;
	Character->Crouch();
	Character->PerformMovement(Scene, ModelTestDeltaTime);
	const float Fall = Character->GetVelocityZ() * ModelTestDeltaTime;
	TestTrue("Crouched", Character->bIsCrouched != 0);
	TestEqual("Feet tucked up", Character->GetActorLocation().Z - Before - Fall,
		92.5f - Character->GetCharacterMovement().CrouchedHalfHeight, 1.0e-2f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementGetMaxSpeedHookTest,
	"System.Engine.CharacterMovement.GetMaxSpeedHook",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementGetMaxSpeedHookTest::RunTest(const FString& Parameters)
{
	// A game's movement class (SetDefaultSubobjectClass) overrides GetMaxSpeed, and both models walk at its speed.
	FScopedTestWorld TestWorld;
	FPhysScene Scene;
	ACharacter* Character = TestWorld->SpawnActor<ACharacter>(AEngineTestCharacter::StaticClass());
	Character->Reset(FVector::ZeroVector);
	UEngineTestCharacterMovement* Move = Cast<UEngineTestCharacterMovement>(&Character->GetCharacterMovement());
	if (!TestNotNull("The game's movement class", Move))
	{
		return false;
	}
	Move->bTestWalking = true;
	RunModelFrames(*Character, Scene, FVector(1.0f, 0.0f, 0.0f), 60);
	TestEqual("Instant model at half speed", Character->GetActorLocation().X, Move->MaxWalkSpeed * 0.5f, 0.5f);

	Move->bInstantVelocity = false;
	RunModelFrames(*Character, Scene, FVector(1.0f, 0.0f, 0.0f), 60);
	TestEqual("UE model at half speed", Move->Velocity.Size(), Move->MaxWalkSpeed * 0.5f, 0.05f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementPawnInputVectorTest,
	"System.Engine.CharacterMovement.PawnInputVector",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementPawnInputVectorTest::RunTest(const FString& Parameters)
{
	// The pawn's input vector (APawn::AddMovementInput, what the player's axes add) moves the character for the tick
	// that consumes it, and stops when no input comes.
	FScopedTestWorld TestWorld;
	ACharacter* Character = TestWorld->SpawnActor<ACharacter>();
	Character->Reset(FVector::ZeroVector);
	const float Speed = Character->GetCharacterMovement().MaxWalkSpeed;
	Character->APawn::AddMovementInput(FVector(1.0f, 0.0f, 0.0f), 1.0f);
	Character->TickCharacterMovement(ModelTestDeltaTime);
	TestEqual("Moved by the input", Character->GetActorLocation().X, Speed * ModelTestDeltaTime, 1.0e-3f);
	Character->TickCharacterMovement(ModelTestDeltaTime);
	TestEqual("Stopped without input", Character->GetActorLocation().X, Speed * ModelTestDeltaTime, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementFirstPersonCameraTest,
	"System.Engine.CharacterMovement.FirstPersonCamera",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementFirstPersonCameraTest::RunTest(const FString& Parameters)
{
	// UE's first-person camera: a camera attached at the eyes with bUsePawnControlRotation looks from its world
	// location along the controller's rotation.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(100.0f, 0.0f, 0.0f));
	UCameraComponent* Camera = NewObject<UCameraComponent>(Character, TEXT("FirstPersonCamera"));
	Camera->bUsePawnControlRotation = true;
	(void)Camera->AttachToComponent(Character->GetCapsuleComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	Camera->RelativeLocation = FVector(0.0f, 0.0f, 160.0f);
	Camera->RegisterComponent();

	APlayerController* Controller = World.SpawnActor<APlayerController>();
	Controller->Possess(Character);
	Controller->SetControlRotation(FRotator(-10.0f, 45.0f, 0.0f));

	FMinimalViewInfo View;
	Character->CalcCamera(0.0f, View);
	TestEqual("Eye X", View.Location.X, 100.0f, 1.0e-3f);
	TestEqual("Eye Z", View.Location.Z, 160.0f, 1.0e-3f);
	TestEqual("Pitch", View.Rotation.Pitch, -10.0f, 1.0e-3f);
	TestEqual("Yaw", View.Rotation.Yaw, 45.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementMouseSensitivityTest,
	"System.Engine.CharacterMovement.MouseSensitivity",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementMouseSensitivityTest::RunTest(const FString& Parameters)
{
	// The mouse sensitivity starts from the input settings' AxisConfig (BaseInput.ini: 0.3) and a player changes it
	// (UE: UPlayerInput::SetMouseSensitivity, an Exec command).
	UPlayerInput* Input = NewObject<UPlayerInput>();
	TestEqual("Config X", Input->GetMouseSensitivityX(), 0.3f, 1.0e-6f);
	Input->SetMouseSensitivity(0.1f, 0.2f);
	TestEqual("Set X", Input->GetMouseSensitivityX(), 0.1f, 1.0e-6f);
	TestEqual("Set Y", Input->GetMouseSensitivityY(), 0.2f, 1.0e-6f);
	TestTrue("Exec", Input->CallFunctionByNameWithArguments(TEXT("SetMouseSensitivity 0.07"), *GLog, nullptr));
	TestEqual("Exec X", Input->GetMouseSensitivityX(), 0.07f, 1.0e-6f);
	TestEqual("Exec Y", Input->GetMouseSensitivityY(), 0.07f, 1.0e-6f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementLongFramesKeepTheFloorTest,
	"System.Engine.CharacterMovement.LongFramesKeepTheFloor",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementLongFramesKeepTheFloorTest::RunTest(const FString& Parameters)
{
	// A slow frame (0.1 s, 0.25 s) steps gravity deeper than the floor's skin: the character stays on a 20 cm slab and
	// on a 1 cm pad over it (de_leon's floor and spawn pads) instead of sinking through; a fall onto the slab from 3 m
	// in 0.1 s frames lands on it.
	for (const float DeltaTime : {1.0f / 60.0f, 0.1f, 0.25f})
	{
		for (const bool bPad : {false, true})
		{
			FScopedTestWorld TestWorld;
			UWorld& World = *TestWorld;
			(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
				FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -10.0f), FVector(80.0f, 80.0f, 0.2f)));
			if (bPad)
			{
				(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
					FTransform(FQuat::Identity, FVector(0.0f, 0.0f, 0.5f), FVector(7.0f, 12.0f, 0.01f)));
			}
			ACharacter* Character =
				World.SpawnActor<ACharacter>(FVector(0.0f, 0.0f, bPad ? 1.0f : 0.0f), FRotator::ZeroRotator);
			for (int32 Frame = 0; Frame < 12; ++Frame)
			{
				World.Tick(DeltaTime);
			}
			const FString Case = FString::Printf(
				TEXT("%.3f s frames%s"), static_cast<double>(DeltaTime), bPad ? TEXT(" on the pad") : TEXT(""));
			TestEqual(*(Case + TEXT(": on the floor")), Character->GetActorLocation().Z, bPad ? 1.0f : 0.0f, 0.01f);
			TestTrue(*(Case + TEXT(": walking")), Character->IsMovingOnGround());
		}
	}

	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	(void)World.SpawnActor<ABlockingVolume>(ABlockingVolume::StaticClass(),
		FTransform(FQuat::Identity, FVector(0.0f, 0.0f, -10.0f), FVector(80.0f, 80.0f, 0.2f)));
	ACharacter* Faller = World.SpawnActor<ACharacter>(FVector(0.0f, 0.0f, 300.0f), FRotator::ZeroRotator);
	for (int32 Frame = 0; Frame < 20; ++Frame)
	{
		World.Tick(0.1f);
	}
	TestEqual("The fall lands on the slab", Faller->GetActorLocation().Z, 0.0f, 0.01f);
	TestTrue("Landed", Faller->IsMovingOnGround());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "Tests/ScopedTestWorld.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	/** Adds a static box body (the character tests' floors, walls and ledges). */
	void AddCharacterTestBox(FPhysScene& Scene, const FVector& Center, const FVector& HalfExtents)
	{
		const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
		FBodyInstance& Body = Scene.GetBodies()[Id];
		Body.Position = Center;
		Body.HalfExtents = HalfExtents;
	}

	constexpr float CharacterTestDeltaTime = 1.0f / 60.0f;

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementIsWalkableUsesWalkableFloorZTest,
	"System.Engine.CharacterMovement.IsWalkableUsesWalkableFloorZ",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementIsWalkableUsesWalkableFloorZTest::RunTest(const FString& Parameters)
{
	// A hit is walkable when its normal's up component reaches WalkableFloorZ.
	FScopedTestWorld TestWorld;
	ACharacter& Character = *TestWorld->SpawnActor<ACharacter>();
	Character.GetCharacterMovement().WalkableFloorZ = 0.71f;

	FHitResult Flat{};
	Flat.bBlockingHit = true;
	Flat.ImpactNormal = FVector(0.0f, 0.0f, 1.0f);
	TestTrue("Flat floor walkable", Character.IsWalkable(Flat));

	FHitResult Steep{};
	Steep.bBlockingHit = true;
	Steep.ImpactNormal = FVector(0.0f, 0.0f, 0.5f); // ~60 degrees, steeper than the default UE walkable slope
	TestFalse("Steep floor not walkable", Character.IsWalkable(Steep));

	Character.GetCharacterMovement().WalkableFloorZ = 0.5f;
	TestTrue("Steep floor walkable with a lower WalkableFloorZ", Character.IsWalkable(Steep));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementFindFloorHitsInfiniteFloorPlaneTest,
	"System.Engine.CharacterMovement.FindFloorHitsInfiniteFloorPlane",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementFindFloorHitsInfiniteFloorPlaneTest::RunTest(const FString& Parameters)
{
	// With no bodies, FindFloor finds the infinite floor plane 1 m below the feet.
	FPhysScene Scene;
	FScopedTestWorld TestWorld;
	ACharacter& Character = *TestWorld->SpawnActor<ACharacter>();
	Character.Reset(FVector(0.0f, 0.0f, 100.0f));
	Character.GetCharacterMovement().FloorZ = 0.0f;

	FFindFloorResult Floor{};
	Character.FindFloor(Scene, Floor, 200.0f, nullptr);
	TestTrue("Blocking hit", Floor.bBlockingHit);
	TestTrue("Walkable floor", Floor.bWalkableFloor);
	TestTrue("Floor plane hit", Floor.Hit.bFloorPlane);
	TestEqual("Impact Z", Floor.Hit.ImpactPoint.Z, 0.0f, 0.1f);
	TestEqual("Floor distance", Floor.FloorDist, 100.0f, 0.1f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementFindFloorHitsStaticAabbTopTest,
	"System.Engine.CharacterMovement.FindFloorHitsStaticAabbTop",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementFindFloorHitsStaticAabbTopTest::RunTest(const FString& Parameters)
{
	// FindFloor prefers the top of a static box under the feet over a far floor plane.
	FPhysScene Scene;
	// Box top at z = 200 cm
	AddCharacterTestBox(Scene, FVector(0.0f, 0.0f, 100.0f), FVector(100.0f, 100.0f, 100.0f));

	FScopedTestWorld TestWorld;
	ACharacter& Character = *TestWorld->SpawnActor<ACharacter>();
	Character.Reset(FVector(0.0f, 0.0f, 250.0f));
	Character.GetCharacterMovement().FloorZ = -10000.0f; // prefer box over far plane

	FFindFloorResult Floor{};
	Character.FindFloor(Scene, Floor, 100.0f, nullptr);
	TestTrue("Blocking hit", Floor.bBlockingHit);
	TestTrue("Walkable floor", Floor.bWalkableFloor);
	TestFalse("Not the floor plane", Floor.Hit.bFloorPlane);
	TestEqual("Impact Z", Floor.Hit.ImpactPoint.Z, 200.0f, 1.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementLandsOnFloorPlaneAfterFallTest,
	"System.Engine.CharacterMovement.LandsOnFloorPlaneAfterFall",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementLandsOnFloorPlaneAfterFallTest::RunTest(const FString& Parameters)
{
	// An airborne character falls and comes to rest walking on the floor plane.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	if (!TestNotNull("Character spawned", Character))
	{
		return false;
	}
	Character->Reset(FVector(0.0f, 0.0f, 200.0f));
	Character->GetCharacterMovement().FloorZ = 0.0f;
	Character->GetCharacterMovement().Gravity = 2400.0f;
	Character->ApplyReplicatedState(FVector(0.0f, 0.0f, 200.0f), FRotator::ZeroRotator, 0.0f, false);
	if (!TestTrue("Starts falling", Character->IsFalling()))
	{
		return false;
	}

	FPhysScene& Scene = World.GetPhysicsScene();
	for (int32 I = 0; I < 180; ++I)
	{
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}

	TestTrue("On ground", Character->IsMovingOnGround());
	TestEqual("Feet on the floor", Character->GetActorLocation().Z, 0.0f, 5.0f);
	TestEqual("Vertical velocity", Character->GetVelocityZ(), 0.0f, 10.0f);
	TestTrue("Walkable floor", Character->GetCurrentFloor().bWalkableFloor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementJumpLeavesGroundThenLandsTest,
	"System.Engine.CharacterMovement.JumpLeavesGroundThenLands",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementJumpLeavesGroundThenLandsTest::RunTest(const FString& Parameters)
{
	// A jump leaves the ground moving up and lands back on the floor.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector::ZeroVector);
	Character->GetCharacterMovement().FloorZ = 0.0f;
	Character->GetCharacterMovement().JumpZVelocity = 700.0f;
	Character->GetCharacterMovement().Gravity = 2400.0f;

	FPhysScene& Scene = World.GetPhysicsScene();
	// Settle on floor.
	for (int32 I = 0; I < 10; ++I)
	{
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}
	if (!TestTrue("Settled on ground", Character->IsMovingOnGround()))
	{
		return false;
	}

	Character->Jump();
	Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	if (!TestTrue("Falling after jump", Character->IsFalling()))
	{
		return false;
	}
	if (!TestTrue("Moving up", Character->GetVelocityZ() > 0.0f))
	{
		return false;
	}

	bool bLanded = false;
	for (int32 I = 0; I < 180; ++I)
	{
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
		if (Character->IsMovingOnGround())
		{
			bLanded = true;
			break;
		}
	}
	if (!TestTrue("Landed", bLanded))
	{
		return false;
	}
	TestEqual("Feet on the floor", Character->GetActorLocation().Z, 0.0f, 5.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementDoesNotWalkThroughStaticWallTest,
	"System.Engine.CharacterMovement.DoesNotWalkThroughStaticWall",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementDoesNotWalkThroughStaticWallTest::RunTest(const FString& Parameters)
{
	// Walking into a tall wall stops the character in front of it.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(-200.0f, 0.0f, 0.0f));
	Character->GetCharacterMovement().FloorZ = 0.0f;
	Character->GetCharacterMovement().MaxWalkSpeed = 600.0f;

	FPhysScene& Scene = World.GetPhysicsScene();
	// Tall wall at x=0
	AddCharacterTestBox(Scene, FVector(0.0f, 0.0f, 100.0f), FVector(25.0f, 200.0f, 100.0f));

	for (int32 I = 0; I < 120; ++I)
	{
		Character->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}

	// Capsule radius 35 cm + wall half X 25 cm: the feet X should stay left of ~-60 cm.
	TestTrue("Stopped before the wall", Character->GetActorLocation().X < -50.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementSlidesAlongWallWithDiagonalWishTest,
	"System.Engine.CharacterMovement.SlidesAlongWallWithDiagonalWish",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementSlidesAlongWallWithDiagonalWishTest::RunTest(const FString& Parameters)
{
	// A diagonal move into a wall is blocked across the wall and slides along it.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(-150.0f, 0.0f, 0.0f));
	Character->GetCharacterMovement().FloorZ = 0.0f;
	Character->GetCharacterMovement().MaxWalkSpeed = 500.0f;

	FPhysScene& Scene = World.GetPhysicsScene();
	// Wall in the YZ plane at x=0
	AddCharacterTestBox(Scene, FVector(0.0f, 0.0f, 100.0f), FVector(25.0f, 400.0f, 100.0f));

	const float Y0 = Character->GetActorLocation().Y;
	for (int32 I = 0; I < 90; ++I)
	{
		Character->AddMovementInput(FVector(1.0f, 1.0f, 0.0f));
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}

	TestTrue("Stopped before the wall", Character->GetActorLocation().X < -50.0f);
	TestTrue("Slid forward in Y", Character->GetActorLocation().Y > Y0 + 50.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementSweepDoesNotTunnelThinWallAtHighSpeedTest,
	"System.Engine.CharacterMovement.SweepDoesNotTunnelThinWallAtHighSpeed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementSweepDoesNotTunnelThinWallAtHighSpeedTest::RunTest(const FString& Parameters)
{
	// The movement sweep stops a very fast character at a thin wall instead of passing through it.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(-100.0f, 0.0f, 0.0f));
	Character->GetCharacterMovement().FloorZ = 0.0f;
	Character->GetCharacterMovement().MaxWalkSpeed = 4000.0f; // far above the normal speed

	FPhysScene& Scene = World.GetPhysicsScene();
	AddCharacterTestBox(Scene, FVector(0.0f, 0.0f, 100.0f), FVector(10.0f, 200.0f, 100.0f));

	for (int32 I = 0; I < 30; ++I)
	{
		Character->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}

	TestTrue("Still before the wall", Character->GetActorLocation().X < 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementStepsUpOntoShortLedgeTest,
	"System.Engine.CharacterMovement.StepsUpOntoShortLedge",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementStepsUpOntoShortLedgeTest::RunTest(const FString& Parameters)
{
	// A ledge lower than MaxStepHeight is stepped onto and walked on.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(-150.0f, 0.0f, 0.0f));
	Character->GetCharacterMovement().FloorZ = 0.0f;
	Character->GetCharacterMovement().MaxWalkSpeed = 500.0f;
	Character->GetCharacterMovement().MaxStepHeight = 35.0f;

	FPhysScene& Scene = World.GetPhysicsScene();
	// Top at z=30 cm (< MaxStepHeight). Long/wide so we stay on the ledge after stepping up.
	AddCharacterTestBox(Scene, FVector(800.0f, 0.0f, 15.0f), FVector(800.0f, 400.0f, 15.0f));

	for (int32 I = 0; I < 120; ++I)
	{
		Character->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}

	TestTrue("Moved onto the ledge", Character->GetActorLocation().X > 20.0f);
	TestTrue("Stepped up", Character->GetActorLocation().Z > 20.0f);
	TestTrue("On ground", Character->IsMovingOnGround());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementDoesNotStepUpTallWallTest,
	"System.Engine.CharacterMovement.DoesNotStepUpTallWall",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementDoesNotStepUpTallWallTest::RunTest(const FString& Parameters)
{
	// A block taller than MaxStepHeight stops the character without a step up.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(-150.0f, 0.0f, 0.0f));
	Character->GetCharacterMovement().FloorZ = 0.0f;
	Character->GetCharacterMovement().MaxWalkSpeed = 500.0f;
	Character->GetCharacterMovement().MaxStepHeight = 35.0f;

	FPhysScene& Scene = World.GetPhysicsScene();
	// Top at z=100 cm (> MaxStepHeight)
	AddCharacterTestBox(Scene, FVector(50.0f, 0.0f, 50.0f), FVector(50.0f, 400.0f, 50.0f));

	for (int32 I = 0; I < 120; ++I)
	{
		Character->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}

	TestTrue("Stopped before the block", Character->GetActorLocation().X < 0.0f);
	TestTrue("Did not step up", Character->GetActorLocation().Z < 15.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementMovementModeWalkingJumpFallingLandTest,
	"System.Engine.CharacterMovement.MovementModeWalkingJumpFallingLand",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementMovementModeWalkingJumpFallingLandTest::RunTest(const FString& Parameters)
{
	// The movement mode goes Walking, Falling after a jump, and Walking again on landing.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector::ZeroVector);
	Character->GetCharacterMovement().FloorZ = 0.0f;
	if (!TestTrue("Starts walking", Character->GetMovementMode() == EMovementMode::Walking))
	{
		return false;
	}

	FPhysScene& Scene = World.GetPhysicsScene();
	Character->Jump();
	Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	if (!TestTrue("Falling mode after jump", Character->GetMovementMode() == EMovementMode::Falling))
	{
		return false;
	}
	if (!TestTrue("Falling after jump", Character->IsFalling()))
	{
		return false;
	}

	bool bLanded = false;
	for (int32 I = 0; I < 180; ++I)
	{
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
		if (Character->GetMovementMode() == EMovementMode::Walking)
		{
			bLanded = true;
			break;
		}
	}
	if (!TestTrue("Landed", bLanded))
	{
		return false;
	}
	TestTrue("On ground", Character->IsMovingOnGround());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementWalksOffLedgeEntersFallingTest,
	"System.Engine.CharacterMovement.WalksOffLedgeEntersFalling",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementWalksOffLedgeEntersFallingTest::RunTest(const FString& Parameters)
{
	// Walking off the edge of a platform switches the character to Falling.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(0.0f, 0.0f, 100.0f));
	Character->GetCharacterMovement().FloorZ = -10000.0f; // no infinite floor under gap
	Character->GetCharacterMovement().MaxWalkSpeed = 600.0f;
	Character->GetCharacterMovement().Gravity = 2400.0f;

	FPhysScene& Scene = World.GetPhysicsScene();
	// Platform top at z=100 cm, ends at x=50 cm
	AddCharacterTestBox(Scene, FVector(0.0f, 0.0f, 50.0f), FVector(50.0f, 50.0f, 50.0f));

	// Settle on platform.
	for (int32 I = 0; I < 20; ++I)
	{
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}
	if (!TestTrue("Settled on the platform", Character->IsMovingOnGround()))
	{
		return false;
	}
	if (!TestEqual("Feet on the platform", Character->GetActorLocation().Z, 100.0f, 5.0f))
	{
		return false;
	}

	for (int32 I = 0; I < 90; ++I)
	{
		Character->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
		if (Character->IsFalling())
		{
			break;
		}
	}
	TestTrue("Falling", Character->IsFalling());
	TestTrue("Falling mode", Character->GetMovementMode() == EMovementMode::Falling);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementWalkShoveMovesDynamicCrateWithoutOverlapTest,
	"System.Engine.CharacterMovement.WalkShoveMovesDynamicCrateWithoutOverlap",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementWalkShoveMovesDynamicCrateWithoutOverlapTest::RunTest(const FString& Parameters)
{
	// Walking into a dynamic crate shoves it forward.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	if (!TestNotNull("Character spawned", Character))
	{
		return false;
	}
	Character->GetCharacterMovement().FloorZ = 0.0f;
	Character->GetCharacterMovement().MaxWalkSpeed = 600.0f;
	Character->GetCharacterMovement().PushStrength = 0.85f;
	Character->Reset(FVector::ZeroVector);

	FPhysScene& Scene = World.GetPhysicsScene();
	const int32 Id = Scene.AddBody({3, EBodyType::Dynamic, 1.0f, true});
	FBodyInstance& Crate = Scene.GetBodies()[Id];
	// Capsule radius ~35 cm; place crate so walking +X contacts the west face.
	Crate.Position = FVector(120.0f, 0.0f, 45.0f);
	Crate.HalfExtents = FVector(40.0f, 40.0f, 45.0f);
	Crate.Mass = 1.0f;
	const float X0 = Crate.Position.X;

	for (int32 I = 0; I < 45; ++I)
	{
		Character->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
		FPhysSceneStepParams Step{};
		Step.DeltaTime = CharacterTestDeltaTime;
		Step.FloorZ = 0.0f;
		Step.Gravity = 2400.0f;
		Scene.Step(Step);
	}

	TestTrue("Crate pushed", Crate.Position.X > X0 + 15.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementWorldSeparatesOverlappingCharacterCapsulesTest,
	"System.Engine.CharacterMovement.WorldSeparatesOverlappingCharacterCapsules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementWorldSeparatesOverlappingCharacterCapsulesTest::RunTest(const FString& Parameters)
{
	// One gameplay frame pushes two overlapping characters at least a capsule diameter apart.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* A = World.SpawnActor<ACharacter>();
	ACharacter* B = World.SpawnActor<ACharacter>();
	if (!TestNotNull("First character spawned", A))
	{
		return false;
	}
	if (!TestNotNull("Second character spawned", B))
	{
		return false;
	}
	A->GetCharacterMovement().FloorZ = 0.0f;
	B->GetCharacterMovement().FloorZ = 0.0f;
	A->Reset(FVector::ZeroVector);
	B->Reset(FVector(10.0f, 0.0f, 0.0f));

	FWorldGameplayFrameParams Frame{};
	Frame.DeltaTime = CharacterTestDeltaTime;
	World.TickGameplayFrame(Frame);

	const float Dx = A->GetActorLocation().X - B->GetActorLocation().X;
	const float Dy = A->GetActorLocation().Y - B->GetActorLocation().Y;
	const float Dist = FMath::Sqrt((Dx * Dx) + (Dy * Dy));
	const float MinDist = A->GetCapsule().GetCapsuleRadius() + B->GetCapsule().GetCapsuleRadius();
	TestTrue("Separated", Dist + 0.1f >= MinDist);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementResolvePawnOverlapIgnoresVerticallySeparatedCapsulesTest,
	"System.Engine.CharacterMovement.ResolvePawnOverlapIgnoresVerticallySeparatedCapsules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementResolvePawnOverlapIgnoresVerticallySeparatedCapsulesTest::RunTest(const FString& Parameters)
{
	// Capsules that overlap in XY but not in height are left where they are.
	FScopedTestWorld TestWorld;
	ACharacter& A = *TestWorld->SpawnActor<ACharacter>();
	ACharacter& B = *TestWorld->SpawnActor<ACharacter>();
	A.Reset(FVector::ZeroVector);
	B.Reset(FVector(5.0f, 0.0f, 300.0f));
	A.ResolvePawnOverlap(B);
	TestEqual("First X unchanged", A.GetActorLocation().X, 0.0f, 1.0e-3f);
	TestEqual("Second X unchanged", B.GetActorLocation().X, 5.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementWalksUpWalkableSlopeRampTest,
	"System.Engine.CharacterMovement.WalksUpWalkableSlopeRamp",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementWalksUpWalkableSlopeRampTest::RunTest(const FString& Parameters)
{
	// A 30 degree ramp is walkable: the character climbs it and stays on a walkable floor.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->GetCharacterMovement().FloorZ = -10000.0f;
	Character->GetCharacterMovement().MaxWalkSpeed = 500.0f;
	Character->GetCharacterMovement().WalkableFloorZ = 0.71f; // ~44 degrees

	FPhysScene& Scene = World.GetPhysicsScene();
	// 30 degree ramp (cos 30 ~ 0.866, walkable). Plane through the origin; z ~ x * tan 30.
	Scene.AddSlopeRamp(FVector::ZeroVector, FVector(800.0f, 200.0f, 800.0f), 30.0f);

	const float X0 = -150.0f;
	const float Z0 = X0 * 0.57735027f; // tan(30 degrees)
	Character->Reset(FVector(X0, 0.0f, Z0 + 5.0f));
	for (int32 I = 0; I < 15; ++I)
	{
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}
	if (!TestTrue("Settled on the ramp", Character->IsMovingOnGround()))
	{
		return false;
	}
	const float ZStart = Character->GetActorLocation().Z;

	for (int32 I = 0; I < 60; ++I)
	{
		Character->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}

	TestTrue("On ground", Character->IsMovingOnGround());
	TestTrue("Moved up the ramp in X", Character->GetActorLocation().X > X0 + 80.0f);
	TestTrue("Climbed", Character->GetActorLocation().Z > ZStart + 35.0f);
	TestTrue("Walkable floor", Character->GetCurrentFloor().bWalkableFloor);
	TestTrue("Floor normal walkable", Character->GetCurrentFloor().Hit.ImpactNormal.Z >= 0.71f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementCannotStandOnSteepSlopeRampTest,
	"System.Engine.CharacterMovement.CannotStandOnSteepSlopeRamp",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementCannotStandOnSteepSlopeRampTest::RunTest(const FString& Parameters)
{
	// A 60 degree ramp is not walkable: the character keeps falling but does not sink through it.
	FScopedTestWorld TestWorld;
	UWorld& World = *TestWorld;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->GetCharacterMovement().FloorZ = -10000.0f;
	Character->GetCharacterMovement().Gravity = 2400.0f;
	Character->GetCharacterMovement().WalkableFloorZ = 0.71f;

	FPhysScene& Scene = World.GetPhysicsScene();
	// 60 degree ramp (cos 60 = 0.5 < WalkableFloorZ)
	Scene.AddSlopeRamp(FVector::ZeroVector, FVector(400.0f, 200.0f, 400.0f), 60.0f);

	Character->ApplyReplicatedState(FVector(0.0f, 0.0f, 200.0f), FRotator::ZeroRotator, 0.0f, false);
	for (int32 I = 0; I < 120; ++I)
	{
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}

	TestTrue("Falling", Character->IsFalling());
	TestFalse("Floor not walkable", Character->GetCurrentFloor().bWalkableFloor);
	// Must rest on / above the steep surface, not tunnel below.
	TestTrue("Above the surface", Character->GetActorLocation().Z > -10.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementAirControlScalesHorizontalMoveWhileFallingTest,
	"System.Engine.CharacterMovement.AirControlScalesHorizontalMoveWhileFalling",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementAirControlScalesHorizontalMoveWhileFallingTest::RunTest(const FString& Parameters)
{
	// While falling, the horizontal move scales with AirControl: none at 0, most of the walk speed at 1.
	// Runs 30 falling frames of +X input and returns the X travelled; false when the character stops falling.
	auto RunAirMove = [this](float AirControl, float& OutDx) -> bool
	{
		FScopedTestWorld TestWorld;
		UWorld& World = *TestWorld;
		ACharacter* Character = World.SpawnActor<ACharacter>();
		Character->Reset(FVector(0.0f, 0.0f, 400.0f));
		Character->GetCharacterMovement().FloorZ = 0.0f;
		Character->GetCharacterMovement().MaxWalkSpeed = 600.0f;
		Character->GetCharacterMovement().Gravity = 2400.0f;
		Character->GetCharacterMovement().AirControl = AirControl;
		// Start airborne high enough that 30 frames stay Falling.
		Character->ApplyReplicatedState(FVector(0.0f, 0.0f, 400.0f), FRotator::ZeroRotator, 0.0f, false);

		FPhysScene& Scene = World.GetPhysicsScene();
		if (!TestTrue("Starts falling", Character->IsFalling()))
		{
			return false;
		}

		const float X0 = Character->GetActorLocation().X;
		for (int32 I = 0; I < 30; ++I)
		{
			if (!TestTrue("Still falling", Character->IsFalling()))
			{
				return false;
			}
			Character->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
			Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
		}
		OutDx = Character->GetActorLocation().X - X0;
		return true;
	};

	float DxZero = 0.0f;
	if (!RunAirMove(0.0f, DxZero))
	{
		return false;
	}
	float DxFull = 0.0f;
	if (!RunAirMove(1.0f, DxFull))
	{
		return false;
	}
	TestEqual("No air control", DxZero, 0.0f, 5.0f);
	TestTrue("Full air control moves", DxFull > 100.0f);
	TestTrue("Full air control moves further", DxFull > DxZero + 50.0f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

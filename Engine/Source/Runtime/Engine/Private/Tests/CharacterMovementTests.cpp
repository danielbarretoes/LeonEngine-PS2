#include "CoreMinimal.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"

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
	ACharacter Character;
	Character.GetCharacterMovement().WalkableFloorZ = 0.71f;

	FHitResult Flat{};
	Flat.bBlockingHit = true;
	Flat.ImpactNormal = FVector(0.0f, 1.0f, 0.0f);
	TestTrue("Flat floor walkable", Character.IsWalkable(Flat));

	FHitResult Steep{};
	Steep.bBlockingHit = true;
	Steep.ImpactNormal = FVector(0.0f, 0.5f, 0.0f); // ~60 degrees, steeper than the default UE walkable slope
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
	// With no bodies, FindFloor finds the infinite floor plane one unit below the feet.
	FPhysScene Scene;
	ACharacter Character;
	Character.Reset(FVector(0.0f, 1.0f, 0.0f), 0.0f);
	Character.GetCharacterMovement().FloorY = 0.0f;

	FFindFloorResult Floor{};
	Character.FindFloor(Scene, Floor, 2.0f, nullptr);
	TestTrue("Blocking hit", Floor.bBlockingHit);
	TestTrue("Walkable floor", Floor.bWalkableFloor);
	TestTrue("Floor plane hit", Floor.Hit.bFloorPlane);
	TestEqual("Impact Y", Floor.Hit.ImpactPoint.Y, 0.0f, 1.0e-3f);
	TestEqual("Floor distance", Floor.FloorDist, 1.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementFindFloorHitsStaticAabbTopTest,
	"System.Engine.CharacterMovement.FindFloorHitsStaticAabbTop",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementFindFloorHitsStaticAabbTopTest::RunTest(const FString& Parameters)
{
	// FindFloor prefers the top of a static box under the feet over a far floor plane.
	FPhysScene Scene;
	// Box top at y = 2
	AddCharacterTestBox(Scene, FVector(0.0f, 1.0f, 0.0f), FVector(1.0f, 1.0f, 1.0f));

	ACharacter Character;
	Character.Reset(FVector(0.0f, 2.5f, 0.0f), 0.0f);
	Character.GetCharacterMovement().FloorY = -100.0f; // prefer box over far plane

	FFindFloorResult Floor{};
	Character.FindFloor(Scene, Floor, 1.0f, nullptr);
	TestTrue("Blocking hit", Floor.bBlockingHit);
	TestTrue("Walkable floor", Floor.bWalkableFloor);
	TestFalse("Not the floor plane", Floor.Hit.bFloorPlane);
	TestEqual("Impact Y", Floor.Hit.ImpactPoint.Y, 2.0f, 1.0e-2f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementLandsOnFloorPlaneAfterFallTest,
	"System.Engine.CharacterMovement.LandsOnFloorPlaneAfterFall",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementLandsOnFloorPlaneAfterFallTest::RunTest(const FString& Parameters)
{
	// An airborne character falls and comes to rest walking on the floor plane.
	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	if (!TestNotNull("Character spawned", Character))
	{
		return false;
	}
	Character->Reset(FVector(0.0f, 2.0f, 0.0f), 0.0f);
	Character->GetCharacterMovement().FloorY = 0.0f;
	Character->GetCharacterMovement().Gravity = 24.0f;
	Character->ApplyReplicatedState(FVector(0.0f, 2.0f, 0.0f), 0.0f, 0.0f, false);
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
	TestEqual("Feet on the floor", Character->GetActorLocation().Y, 0.0f, 0.05f);
	TestEqual("Vertical velocity", Character->GetVelocityZ(), 0.0f, 0.1f);
	TestTrue("Walkable floor", Character->GetCurrentFloor().bWalkableFloor);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementJumpLeavesGroundThenLandsTest,
	"System.Engine.CharacterMovement.JumpLeavesGroundThenLands",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementJumpLeavesGroundThenLandsTest::RunTest(const FString& Parameters)
{
	// A jump leaves the ground moving up and lands back on the floor.
	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector::ZeroVector, 0.0f);
	Character->GetCharacterMovement().FloorY = 0.0f;
	Character->GetCharacterMovement().JumpZVelocity = 7.0f;
	Character->GetCharacterMovement().Gravity = 24.0f;

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
	TestEqual("Feet on the floor", Character->GetActorLocation().Y, 0.0f, 0.05f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementDoesNotWalkThroughStaticWallTest,
	"System.Engine.CharacterMovement.DoesNotWalkThroughStaticWall",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementDoesNotWalkThroughStaticWallTest::RunTest(const FString& Parameters)
{
	// Walking into a tall wall stops the character in front of it.
	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(-2.0f, 0.0f, 0.0f), 0.0f);
	Character->GetCharacterMovement().FloorY = 0.0f;
	Character->GetCharacterMovement().MaxWalkSpeed = 6.0f;

	FPhysScene& Scene = World.GetPhysicsScene();
	// Tall wall at x=0
	AddCharacterTestBox(Scene, FVector(0.0f, 1.0f, 0.0f), FVector(0.25f, 1.0f, 2.0f));

	for (int32 I = 0; I < 120; ++I)
	{
		Character->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}

	// Capsule radius 0.35 + wall half X 0.25: the feet X should stay left of ~-0.6.
	TestTrue("Stopped before the wall", Character->GetActorLocation().X < -0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementSlidesAlongWallWithDiagonalWishTest,
	"System.Engine.CharacterMovement.SlidesAlongWallWithDiagonalWish",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementSlidesAlongWallWithDiagonalWishTest::RunTest(const FString& Parameters)
{
	// A diagonal move into a wall is blocked across the wall and slides along it.
	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(-1.5f, 0.0f, 0.0f), 0.0f);
	Character->GetCharacterMovement().FloorY = 0.0f;
	Character->GetCharacterMovement().MaxWalkSpeed = 5.0f;

	FPhysScene& Scene = World.GetPhysicsScene();
	AddCharacterTestBox(Scene, FVector(0.0f, 1.0f, 0.0f), FVector(0.25f, 1.0f, 4.0f)); // wall in YZ plane at x=0

	const float Z0 = Character->GetActorLocation().Z;
	for (int32 I = 0; I < 90; ++I)
	{
		Character->AddMovementInput(FVector(1.0f, 0.0f, 1.0f));
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}

	TestTrue("Stopped before the wall", Character->GetActorLocation().X < -0.5f);
	TestTrue("Slid forward in Z", Character->GetActorLocation().Z > Z0 + 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementSweepDoesNotTunnelThinWallAtHighSpeedTest,
	"System.Engine.CharacterMovement.SweepDoesNotTunnelThinWallAtHighSpeed",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementSweepDoesNotTunnelThinWallAtHighSpeedTest::RunTest(const FString& Parameters)
{
	// The movement sweep stops a very fast character at a thin wall instead of passing through it.
	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(-1.0f, 0.0f, 0.0f), 0.0f);
	Character->GetCharacterMovement().FloorY = 0.0f;
	Character->GetCharacterMovement().MaxWalkSpeed = 40.0f; // far above the normal speed

	FPhysScene& Scene = World.GetPhysicsScene();
	AddCharacterTestBox(Scene, FVector(0.0f, 1.0f, 0.0f), FVector(0.1f, 1.0f, 2.0f));

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
	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(-1.5f, 0.0f, 0.0f), 0.0f);
	Character->GetCharacterMovement().FloorY = 0.0f;
	Character->GetCharacterMovement().MaxWalkSpeed = 5.0f;
	Character->GetCharacterMovement().MaxStepHeight = 0.35f;

	FPhysScene& Scene = World.GetPhysicsScene();
	// Top at y=0.30 (< MaxStepHeight). Long/wide so we stay on the ledge after stepping up.
	AddCharacterTestBox(Scene, FVector(8.0f, 0.15f, 0.0f), FVector(8.0f, 0.15f, 4.0f));

	for (int32 I = 0; I < 120; ++I)
	{
		Character->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}

	TestTrue("Moved onto the ledge", Character->GetActorLocation().X > 0.2f);
	TestTrue("Stepped up", Character->GetActorLocation().Y > 0.2f);
	TestTrue("On ground", Character->IsMovingOnGround());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementDoesNotStepUpTallWallTest,
	"System.Engine.CharacterMovement.DoesNotStepUpTallWall",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementDoesNotStepUpTallWallTest::RunTest(const FString& Parameters)
{
	// A block taller than MaxStepHeight stops the character without a step up.
	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(-1.5f, 0.0f, 0.0f), 0.0f);
	Character->GetCharacterMovement().FloorY = 0.0f;
	Character->GetCharacterMovement().MaxWalkSpeed = 5.0f;
	Character->GetCharacterMovement().MaxStepHeight = 0.35f;

	FPhysScene& Scene = World.GetPhysicsScene();
	// Top at y=1.0 (> MaxStepHeight)
	AddCharacterTestBox(Scene, FVector(0.5f, 0.5f, 0.0f), FVector(0.5f, 0.5f, 4.0f));

	for (int32 I = 0; I < 120; ++I)
	{
		Character->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}

	TestTrue("Stopped before the block", Character->GetActorLocation().X < 0.0f);
	TestTrue("Did not step up", Character->GetActorLocation().Y < 0.15f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementMovementModeWalkingJumpFallingLandTest,
	"System.Engine.CharacterMovement.MovementModeWalkingJumpFallingLand",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementMovementModeWalkingJumpFallingLandTest::RunTest(const FString& Parameters)
{
	// The movement mode goes Walking, Falling after a jump, and Walking again on landing.
	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector::ZeroVector, 0.0f);
	Character->GetCharacterMovement().FloorY = 0.0f;
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
	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(0.0f, 1.0f, 0.0f), 0.0f);
	Character->GetCharacterMovement().FloorY = -100.0f; // no infinite floor under gap
	Character->GetCharacterMovement().MaxWalkSpeed = 6.0f;
	Character->GetCharacterMovement().Gravity = 24.0f;

	FPhysScene& Scene = World.GetPhysicsScene();
	// Platform top at y=1, ends at x=0.5
	AddCharacterTestBox(Scene, FVector(0.0f, 0.5f, 0.0f), FVector(0.5f, 0.5f, 0.5f));

	// Settle on platform.
	for (int32 I = 0; I < 20; ++I)
	{
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}
	if (!TestTrue("Settled on the platform", Character->IsMovingOnGround()))
	{
		return false;
	}
	if (!TestEqual("Feet on the platform", Character->GetActorLocation().Y, 1.0f, 0.05f))
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
	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	if (!TestNotNull("Character spawned", Character))
	{
		return false;
	}
	Character->GetCharacterMovement().FloorY = 0.0f;
	Character->GetCharacterMovement().MaxWalkSpeed = 6.0f;
	Character->GetCharacterMovement().PushStrength = 0.85f;
	Character->Reset(FVector::ZeroVector, 0.0f);

	FPhysScene& Scene = World.GetPhysicsScene();
	const int32 Id = Scene.AddBody({3, EBodyType::Dynamic, 1.0f, true});
	FBodyInstance& Crate = Scene.GetBodies()[Id];
	// Capsule radius ~0.35; place crate so walking +X contacts the west face.
	Crate.Position = FVector(1.2f, 0.45f, 0.0f);
	Crate.HalfExtents = FVector(0.4f, 0.45f, 0.4f);
	Crate.Mass = 1.0f;
	const float X0 = Crate.Position.X;

	for (int32 I = 0; I < 45; ++I)
	{
		Character->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
		FPhysSceneStepParams Step{};
		Step.DeltaTime = CharacterTestDeltaTime;
		Step.FloorY = 0.0f;
		Step.Gravity = 24.0f;
		Scene.Step(Step);
	}

	TestTrue("Crate pushed", Crate.Position.X > X0 + 0.15f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementWorldSeparatesOverlappingCharacterCapsulesTest,
	"System.Engine.CharacterMovement.WorldSeparatesOverlappingCharacterCapsules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementWorldSeparatesOverlappingCharacterCapsulesTest::RunTest(const FString& Parameters)
{
	// One gameplay frame pushes two overlapping characters at least a capsule diameter apart.
	UWorld World;
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
	A->GetCharacterMovement().FloorY = 0.0f;
	B->GetCharacterMovement().FloorY = 0.0f;
	A->Reset(FVector::ZeroVector, 0.0f);
	B->Reset(FVector(0.1f, 0.0f, 0.0f), 0.0f);

	FWorldGameplayFrameParams Frame{};
	Frame.DeltaTime = CharacterTestDeltaTime;
	World.TickGameplayFrame(Frame);

	const float Dx = A->GetActorLocation().X - B->GetActorLocation().X;
	const float Dz = A->GetActorLocation().Z - B->GetActorLocation().Z;
	const float Dist = FMath::Sqrt((Dx * Dx) + (Dz * Dz));
	const float MinDist = A->GetCapsule().GetCapsuleRadius() + B->GetCapsule().GetCapsuleRadius();
	TestTrue("Separated", Dist + 1.0e-3f >= MinDist);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementResolvePawnOverlapIgnoresVerticallySeparatedCapsulesTest,
	"System.Engine.CharacterMovement.ResolvePawnOverlapIgnoresVerticallySeparatedCapsules",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementResolvePawnOverlapIgnoresVerticallySeparatedCapsulesTest::RunTest(const FString& Parameters)
{
	// Capsules that overlap in XZ but not in height are left where they are.
	ACharacter A;
	ACharacter B;
	A.Reset(FVector::ZeroVector, 0.0f);
	B.Reset(FVector(0.05f, 3.0f, 0.0f), 0.0f);
	A.ResolvePawnOverlap(B);
	TestEqual("First X unchanged", A.GetActorLocation().X, 0.0f, 1.0e-5f);
	TestEqual("Second X unchanged", B.GetActorLocation().X, 0.05f, 1.0e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementWalksUpWalkableSlopeRampTest,
	"System.Engine.CharacterMovement.WalksUpWalkableSlopeRamp",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementWalksUpWalkableSlopeRampTest::RunTest(const FString& Parameters)
{
	// A 30 degree ramp is walkable: the character climbs it and stays on a walkable floor.
	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->GetCharacterMovement().FloorY = -100.0f;
	Character->GetCharacterMovement().MaxWalkSpeed = 5.0f;
	Character->GetCharacterMovement().WalkableFloorZ = 0.71f; // ~44 degrees

	FPhysScene& Scene = World.GetPhysicsScene();
	// 30 degree ramp (cos 30 ~ 0.866, walkable). Plane through the origin; y ~ x * tan 30.
	Scene.AddSlopeRamp(FVector::ZeroVector, FVector(8.0f, 8.0f, 2.0f), 30.0f);

	const float X0 = -1.5f;
	const float Y0 = X0 * 0.57735027f; // tan(30 degrees)
	Character->Reset(FVector(X0, Y0 + 0.05f, 0.0f), 0.0f);
	for (int32 I = 0; I < 15; ++I)
	{
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}
	if (!TestTrue("Settled on the ramp", Character->IsMovingOnGround()))
	{
		return false;
	}
	const float YStart = Character->GetActorLocation().Y;

	for (int32 I = 0; I < 60; ++I)
	{
		Character->AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}

	TestTrue("On ground", Character->IsMovingOnGround());
	TestTrue("Moved up the ramp in X", Character->GetActorLocation().X > X0 + 0.8f);
	TestTrue("Climbed", Character->GetActorLocation().Y > YStart + 0.35f);
	TestTrue("Walkable floor", Character->GetCurrentFloor().bWalkableFloor);
	TestTrue("Floor normal walkable", Character->GetCurrentFloor().Hit.ImpactNormal.Y >= 0.71f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCharacterMovementCannotStandOnSteepSlopeRampTest,
	"System.Engine.CharacterMovement.CannotStandOnSteepSlopeRamp",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FCharacterMovementCannotStandOnSteepSlopeRampTest::RunTest(const FString& Parameters)
{
	// A 60 degree ramp is not walkable: the character keeps falling but does not sink through it.
	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->GetCharacterMovement().FloorY = -100.0f;
	Character->GetCharacterMovement().Gravity = 24.0f;
	Character->GetCharacterMovement().WalkableFloorZ = 0.71f;

	FPhysScene& Scene = World.GetPhysicsScene();
	// 60 degree ramp (cos 60 = 0.5 < WalkableFloorZ)
	Scene.AddSlopeRamp(FVector::ZeroVector, FVector(4.0f, 4.0f, 2.0f), 60.0f);

	Character->ApplyReplicatedState(FVector(0.0f, 2.0f, 0.0f), 0.0f, 0.0f, false);
	for (int32 I = 0; I < 120; ++I)
	{
		Character->PerformMovement(Scene, CharacterTestDeltaTime, nullptr);
	}

	TestTrue("Falling", Character->IsFalling());
	TestFalse("Floor not walkable", Character->GetCurrentFloor().bWalkableFloor);
	// Must rest on / above the steep surface, not tunnel below.
	TestTrue("Above the surface", Character->GetActorLocation().Y > -0.1f);
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
		UWorld World;
		ACharacter* Character = World.SpawnActor<ACharacter>();
		Character->Reset(FVector(0.0f, 4.0f, 0.0f), 0.0f);
		Character->GetCharacterMovement().FloorY = 0.0f;
		Character->GetCharacterMovement().MaxWalkSpeed = 6.0f;
		Character->GetCharacterMovement().Gravity = 24.0f;
		Character->GetCharacterMovement().AirControl = AirControl;
		// Start airborne high enough that 30 frames stay Falling.
		Character->ApplyReplicatedState(FVector(0.0f, 4.0f, 0.0f), 0.0f, 0.0f, false);

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
	TestEqual("No air control", DxZero, 0.0f, 0.05f);
	TestTrue("Full air control moves", DxFull > 1.0f);
	TestTrue("Full air control moves further", DxFull > DxZero + 0.5f);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

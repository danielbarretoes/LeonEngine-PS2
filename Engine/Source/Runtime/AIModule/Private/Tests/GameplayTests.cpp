#include "AI/Navigation/NavigationSystem.h"
#include "AIController.h"
#include "BodyInstance.h"
#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "Debug/DebugDraw.h"
#include "Engine/GameInstance.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Controller.h"
#include "GameFramework/DefaultGameMode.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/SpringArmComponent.h"
#include "Misc/AutomationTest.h"
#include "Physics/PhysScene.h"
#include "PhysicsBackend.h"
#include "TriangleCollision.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{

	class ATestActor : public AActor
	{
	};
	class ATestPawn : public APawn
	{
	};
	class ATestController : public AController
	{
	};

} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayWorldSpawnsTicksAndDestroysActorsTest,
	"System.AIModule.Gameplay.WorldSpawnsTicksAndDestroysActors",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayWorldSpawnsTicksAndDestroysActorsTest::RunTest(const FString& Parameters)
{
	// A spawned Actor belongs to its World, is purged on the Tick after Destroy, and Clear empties the World.
	UWorld World;
	ATestActor* Actor = World.SpawnActor<ATestActor>();
	if (!TestNotNull("Spawned actor", Actor))
	{
		return false;
	}
	TestEqual("One actor", World.ActorCount(), static_cast<SIZE_T>(1));
	TestTrue("Actor world", Actor->GetWorld() == &World);

	Actor->SetActorLocation(FVector(100.0f, 200.0f, 300.0f));
	TestEqual("Location Y", Actor->GetActorLocation().Y, 200.0f, 1.0e-3f);

	World.DestroyActor(Actor);
	TestTrue("Pending kill", Actor->IsPendingKillPending());
	World.Tick(0.016f);
	TestEqual("Purged after tick", World.ActorCount(), static_cast<SIZE_T>(0));

	World.SpawnActor<ATestActor>();
	World.Clear();
	TestEqual("Empty after clear", World.ActorCount(), static_cast<SIZE_T>(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayWorldFindFirstFindsDerivedTypeTest,
	"System.AIModule.Gameplay.WorldFindFirstFindsDerivedType",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayWorldFindFirstFindsDerivedTypeTest::RunTest(const FString& Parameters)
{
	// FindFirst returns the first Actor of the requested type and null when none matches.
	UWorld World;
	World.SpawnActor<ATestActor>();
	ATestPawn* Pawn = World.SpawnActor<ATestPawn>();
	TestTrue("Finds the pawn", World.FindFirst<ATestPawn>() == Pawn);
	TestNull("No character", World.FindFirst<ACharacter>());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayControllerPossessAndUnPossessTest,
	"System.AIModule.Gameplay.ControllerPossessAndUnPossess",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayControllerPossessAndUnPossessTest::RunTest(const FString& Parameters)
{
	// Possess links the Controller and the Pawn both ways; UnPossess clears both links.
	UWorld World;
	ATestPawn* Pawn = World.SpawnActor<ATestPawn>();
	ATestController Controller;
	Controller.Possess(Pawn);
	TestTrue("Controller has pawn", Controller.HasPawn());
	TestTrue("Pawn possessed", Pawn->IsPossessed());
	TestTrue("Pawn controller", Pawn->GetController() == &Controller);

	Controller.UnPossess();
	TestFalse("Controller has no pawn", Controller.HasPawn());
	TestFalse("Pawn not possessed", Pawn->IsPossessed());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayPawnDestroyUnPossessesControllerTest,
	"System.AIModule.Gameplay.PawnDestroyUnPossessesController",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayPawnDestroyUnPossessesControllerTest::RunTest(const FString& Parameters)
{
	// Destroying a possessed Pawn releases its Controller.
	UWorld World;
	ATestPawn* Pawn = World.SpawnActor<ATestPawn>();
	ATestController Controller;
	Controller.Possess(Pawn);
	Pawn->Destroy();
	World.Tick(0.0f);
	TestFalse("Controller released", Controller.HasPawn());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayGameStateMatchTimerAndPlayerStateScoreTest,
	"System.AIModule.Gameplay.GameStateMatchTimerAndPlayerStateScore",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayGameStateMatchTimerAndPlayerStateScoreTest::RunTest(const FString& Parameters)
{
	// The match clock runs only while the match is in progress; Reset clears the clock and the player score.
	AGameStateBase GameState;
	GameState.HandleMatchHasStarted();
	GameState.Tick(0.5f);
	TestTrue("Match started", GameState.HasMatchStarted());
	TestEqual("Clock advanced", GameState.GetServerWorldTimeSeconds(), 0.5f, 1.0e-5f);
	GameState.Reset();
	TestEqual("Clock reset", GameState.GetServerWorldTimeSeconds(), 0.0f, 1.0e-5f);
	TestFalse("Match reset", GameState.HasMatchStarted());

	APlayerState PlayerState;
	PlayerState.SetPlayerId(2);
	PlayerState.SetPlayerName("P2");
	PlayerState.AddScore(10.0f);
	TestEqual("Player id", PlayerState.GetPlayerId(), 2);
	TestEqual("Player name", PlayerState.GetPlayerName(), "P2");
	TestEqual("Score", PlayerState.GetScore(), 10.0f, 1.0e-5f);
	PlayerState.Reset();
	TestEqual("Score reset", PlayerState.GetScore(), 0.0f, 1.0e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayGameInstanceNotifyLevelOpenedTest,
	"System.AIModule.Gameplay.GameInstanceNotifyLevelOpened",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayGameInstanceNotifyLevelOpenedTest::RunTest(const FString& Parameters)
{
	// Each NotifyLevelOpened bumps the opened-level counter.
	UGameInstance GameInstance;
	TestEqual("No levels yet", GameInstance.GetLevelsOpened(), 0);
	GameInstance.NotifyLevelOpened();
	GameInstance.NotifyLevelOpened();
	TestEqual("Two levels", GameInstance.GetLevelsOpened(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplaySpringArmClampsPitchAndArmLengthTest,
	"System.AIModule.Gameplay.SpringArmComponentClampsPitchAndArmLength",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplaySpringArmClampsPitchAndArmLengthTest::RunTest(const FString& Parameters)
{
	// Pitch and arm length inputs clamp to their limits, and the boom drives an orbit camera at the socket height.
	USpringArmComponent Arm;
	Arm.AddPitchInput(200.0f);
	TestTrue("Pitch clamped to max", Arm.BoomPitchDegrees <= Arm.PitchMax);
	Arm.AddPitchInput(-400.0f);
	TestTrue("Pitch clamped to min", Arm.BoomPitchDegrees >= Arm.PitchMin);

	Arm.AddArmLengthInput(10000.0f);
	TestEqual("Arm length clamped to max", Arm.TargetArmLength, Arm.ArmLengthMax, 1.0e-3f);

	Arm.SnapLagState(FVector::ZeroVector);
	UCameraComponent Camera;
	Arm.ApplyToCamera(Camera, FVector(100.0f, 0.0f, 0.0f), 0.016f);
	TestTrue("Orbit camera", Camera.GetMode() == ECameraMode::Orbit);
	TestEqual("Camera target height", Camera.GetTarget().Z, Arm.SocketOffsetZ, 50.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplaySpringArmCollisionProbeShortensArmTest,
	"System.AIModule.Gameplay.SpringArmComponentCollisionProbeShortensArm",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplaySpringArmCollisionProbeShortensArmTest::RunTest(const FString& Parameters)
{
	// A static box between the pawn and the camera pulls the arm in, but never below ArmLengthMin.
	FPhysScene Scene;
	const int32 Id = Scene.AddBody({0, EBodyType::Static, 1.0f, true});
	Scene.GetBodies()[Id].Position = FVector(200.0f, 0.0f, 100.0f);
	Scene.GetBodies()[Id].HalfExtents = FVector(25.0f, 200.0f, 100.0f);

	USpringArmComponent Arm;
	Arm.bDoCollisionTest = true;
	Arm.bEnableCameraLag = false;
	Arm.bEnableCameraRotationLag = false;
	Arm.ArmLengthLagSpeed = 1000.0f;
	Arm.TargetArmLength = 600.0f;
	Arm.ArmLengthMin = 50.0f;
	Arm.BoomYawDegrees = 0.0f;
	Arm.BoomPitchDegrees = 0.0f;
	Arm.SocketOffsetZ = 100.0f;
	Arm.SocketOffsetX = 0.0f;
	Arm.ProbeSize = 15.0f;
	Arm.CollisionProbeOffset = 5.0f;
	Arm.SnapLagState(FVector::ZeroVector);

	UCameraComponent Camera;
	Arm.ApplyToCamera(Camera, FVector::ZeroVector, 0.016f, &Scene);
	TestTrue("Arm shortened", Camera.GetDistance() < 300.0f);
	TestTrue("Arm above minimum", Camera.GetDistance() >= Arm.ArmLengthMin);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayAIControllerSteersTowardTargetAndArrivesTest,
	"System.AIModule.Gameplay.AIControllerSteersTowardTargetAndArrives",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayAIControllerSteersTowardTargetAndArrivesTest::RunTest(const FString& Parameters)
{
	// Far from the target the wish is a unit vector toward it; inside the arrive radius it is zero.
	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(0.0f, 0.0f, 0.0f));

	AAIController Ai;
	Ai.Possess(Character);
	Ai.SetArriveRadius(50.0f);
	Ai.MoveToLocation(FVector(1000.0f, 0.0f, 0.0f));

	const FVector WishFar = Ai.TickAI(0.016f);
	TestEqual("Unit wish when far", WishFar.Size(), 1.0f, 1.0e-3f);
	TestTrue("Wish points at target", WishFar.X > 0.5f);

	Character->Reset(FVector(1000.0f, 0.0f, 0.0f));
	const FVector WishNear = Ai.TickAI(0.016f);
	TestEqual("No wish on arrival", WishNear.Size(), 0.0f, 1.0e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayAIControllerMoveToActorTracksMovingTargetTest,
	"System.AIModule.Gameplay.AIControllerMoveToActorTracksMovingTarget",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayAIControllerMoveToActorTracksMovingTargetTest::RunTest(const FString& Parameters)
{
	// MoveToActor follows the target Actor's current location and stops once it is within the arrive radius.
	UWorld World;
	ACharacter* Hunter = World.SpawnActor<ACharacter>();
	ACharacter* Prey = World.SpawnActor<ACharacter>();
	Hunter->Reset(FVector(0.0f, 0.0f, 0.0f));
	Prey->Reset(FVector(800.0f, 0.0f, 0.0f));

	AAIController Ai;
	Ai.Possess(Hunter);
	Ai.SetArriveRadius(40.0f);
	Ai.MoveToActor(Prey);

	const FVector Wish = Ai.TickAI(0.016f);
	TestTrue("Wish points at prey", Wish.X > 0.5f);
	TestTrue("Move actor is prey", Ai.GetMoveActor() == Prey);

	Prey->Reset(FVector(20.0f, 0.0f, 0.0f));
	const FVector WishArrived = Ai.TickAI(0.016f);
	TestEqual("No wish on arrival", WishArrived.Size(), 0.0f, 1.0e-5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayAIControllerPathFollowDoesNotShortcutTest,
	"System.AIModule.Gameplay.AIControllerPathFollowDoesNotShortcutThroughBlocker",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayAIControllerPathFollowDoesNotShortcutTest::RunTest(const FString& Parameters)
{
	// With a NavMesh and a wall in the way, the first steering step follows the detour instead of charging the wall.
	FPhysScene Physics;
	FBodyInstance Wall{};
	Wall.Type = EBodyType::Static;
	Wall.Position = FVector(0.0f, 0.0f, 100.0f);
	Wall.HalfExtents = FVector(60.0f, 400.0f, 150.0f);
	Physics.GetBodies().Add(Wall);

	UNavigationSystem Nav;
	Nav.SetCellSize(50.0f);
	Nav.SetAgentRadius(45.0f);
	Nav.BuildFromPhysScene(Physics, 0.0f, 1200.0f);
	if (!TestTrue("Nav mesh built", Nav.HasNavMesh()))
	{
		return false;
	}

	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->Reset(FVector(-500.0f, 0.0f, 0.0f));

	AAIController Ai;
	Ai.Possess(Character);
	Ai.SetNavigationSystem(&Nav);
	// CoopTp-like large goal arrive: must not skip detour waypoints through the wall.
	Ai.SetArriveRadius(125.0f);
	Ai.MoveToLocation(FVector(500.0f, 0.0f, 0.0f));
	if (!TestTrue("Following a path", Ai.IsFollowingPath()))
	{
		return false;
	}
	TestTrue("Path has a detour", Ai.PathPoints().Num() >= 3);

	const FVector Wish = Ai.TickAI(0.016f);
	TestTrue("Moving", Wish.Size() > 0.5f);
	// Detour is off the X axis (around the wall), not a pure +X charge through it.
	TestTrue("Steering around the wall", FMath::Abs(Wish.Y) > 0.35f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayNavFindPathRoutesAroundStaticBlockerTest,
	"System.AIModule.Gameplay.NavigationSystemFindPathRoutesAroundStaticBlocker",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayNavFindPathRoutesAroundStaticBlockerTest::RunTest(const FString& Parameters)
{
	// A floor slab stays walkable, a wall blocks, FindPath detours around the wall and projection snaps to the floor.
	FPhysScene Physics;

	// Floor plane-like slab (wide aspect) must NOT wipe the whole grid.
	FBodyInstance Floor{};
	Floor.Type = EBodyType::Static;
	Floor.Position = FVector(0.0f, 0.0f, 0.0f);
	Floor.HalfExtents = FVector(2000.0f, 2000.0f, 50.0f);
	Physics.GetBodies().Add(Floor);

	FBodyInstance Wall{};
	Wall.Type = EBodyType::Static;
	Wall.Position = FVector(0.0f, 0.0f, 100.0f);
	Wall.HalfExtents = FVector(60.0f, 500.0f, 150.0f);
	Physics.GetBodies().Add(Wall);

	UNavigationSystem Nav;
	Nav.SetCellSize(50.0f);
	Nav.SetAgentRadius(35.0f);
	Nav.BuildFromPhysScene(Physics, 0.0f, 1200.0f);
	if (!TestTrue("Nav mesh built", Nav.HasNavMesh()))
	{
		return false;
	}
	TestTrue("Floor walkable", Nav.GetWalkableCellCount() > 100);
	TestEqual("One blocker", Nav.GetBlockerCount(), 1);

	TArray<FVector> Path;
	if (!TestTrue("Path found", Nav.FindPath(FVector(-600.0f, 0.0f, 0.0f), FVector(600.0f, 0.0f, 0.0f), Path)))
	{
		return false;
	}
	TestTrue("Path has a detour", Path.Num() >= 3);

	bool bDetoured = false;
	for (const FVector& Point : Path)
	{
		if (FMath::Abs(Point.Y) > 125.0f)
		{
			bDetoured = true;
			break;
		}
	}
	TestTrue("Path leaves the X axis", bDetoured);

	FVector Projected = FVector::ZeroVector;
	if (!TestTrue("Point projected", Nav.ProjectPointToNavigation(FVector(-600.0f, 0.0f, 200.0f), Projected)))
	{
		return false;
	}
	TestEqual("Projected to the floor", Projected.Z, 0.0f, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayNavBlocksNavBlockerKeepsNavWalkableTest,
	"System.AIModule.Gameplay.NavigationSystemBlocksNavBlockerButKeepsNavWalkableWalkable",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayNavBlocksNavBlockerKeepsNavWalkableTest::RunTest(const FString& Parameters)
{
	// A NavBlocker-tagged plate blocks its cells and forces a detour; a NavWalkable ramp mesh stays walkable.
	ULevel Level;
	FPhysScene Physics;

	UStaticMeshComponent Plate{};
	Plate.Tag = NavTags::Blocker;
	Plate.bCollisionEnabled = true;
	Plate.EditorClass = "Cube";
	Plate.Transform = FTransform(FQuat::Identity, FVector(0.0f, 0.0f, 12.0f), FVector(1.8f, 1.8f, 0.2f));
	Level.GetStaticMeshes().Add(MoveTemp(Plate));

	FBodyInstance PlateBody{};
	PlateBody.Type = EBodyType::Static;
	PlateBody.LevelMeshIndex = 0;
	PlateBody.Position = FVector(0.0f, 0.0f, 12.0f);
	PlateBody.HalfExtents = FVector(90.0f, 90.0f, 10.0f);
	Physics.GetBodies().Add(PlateBody);
	Physics.GetTriangleMeshes().AddDefaulted();

	UStaticMeshComponent Ramp{};
	Ramp.Tag = NavTags::Walkable;
	Ramp.bCollisionEnabled = true;
	Ramp.EditorClass = "Cube";
	Level.GetStaticMeshes().Add(MoveTemp(Ramp));

	FBodyInstance RampBody{};
	RampBody.Type = EBodyType::Static;
	RampBody.LevelMeshIndex = 1;
	RampBody.Position = FVector(400.0f, 0.0f, 100.0f);
	RampBody.HalfExtents = FVector(250.0f, 120.0f, 100.0f);
	RampBody.CollisionShape = EBodyCollisionShape::TriangleMesh;
	Physics.GetBodies().Add(RampBody);

	FTriangleMeshCollision Tri{};
	// Two tris covering a 4 x 2 m footprint around (400, 0) cm.
	Tri.Positions = {FVector(200.0f, -100.0f, 50.0f), FVector(600.0f, -100.0f, 150.0f), FVector(600.0f, 100.0f, 150.0f),
		FVector(200.0f, 100.0f, 50.0f)};
	Tri.Indices = {0, 1, 2, 0, 2, 3};
	Physics.GetTriangleMeshes().Add(MoveTemp(Tri));

	UNavigationSystem Nav;
	Nav.SetCellSize(50.0f);
	Nav.SetAgentRadius(35.0f);
	Nav.BuildFromLevel(Level, Physics, 0.0f, 1200.0f);
	if (!TestTrue("Nav mesh built", Nav.HasNavMesh()))
	{
		return false;
	}
	TestEqual("One blocker", Nav.GetBlockerCount(), 1);

	// Cell under plate center must be blocked.
	int Pix = 0;
	int Piy = 0;
	TestTrue("Plate cell found", Nav.GetNavMesh().WorldToCell(0.0f, 0.0f, Pix, Piy));
	TestFalse("Plate cell blocked", Nav.GetNavMesh().IsWalkable(Pix, Piy));

	// Path across the plate must detour.
	TArray<FVector> Path;
	if (!TestTrue(
			"Path across the plate", Nav.FindPath(FVector(-300.0f, 0.0f, 0.0f), FVector(300.0f, 0.0f, 0.0f), Path)))
	{
		return false;
	}
	bool bDetouredPlate = false;
	for (const FVector& Point : Path)
	{
		if (FMath::Abs(Point.Y) > 80.0f)
		{
			bDetouredPlate = true;
			break;
		}
	}
	TestTrue("Path detours around the plate", bDetouredPlate);

	// Climbable ramp footprint stays walkable (CMC handles the slope).
	int Rix = 0;
	int Riy = 0;
	TestTrue("Ramp cell found", Nav.GetNavMesh().WorldToCell(400.0f, 0.0f, Rix, Riy));
	TestTrue("Ramp cell walkable", Nav.GetNavMesh().IsWalkable(Rix, Riy));
	TestTrue("Path over the ramp", Nav.FindPath(FVector(200.0f, 0.0f, 0.0f), FVector(600.0f, 0.0f, 0.0f), Path));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayNavAppendDebugDrawFillsOverlayTest,
	"System.AIModule.Gameplay.NavigationSystemAppendDebugDrawFillsOverlay",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayNavAppendDebugDrawFillsOverlayTest::RunTest(const FString& Parameters)
{
	// Drawing a baked NavMesh adds lines to an empty debug draw batch.
	FPhysScene Physics;
	FBodyInstance Wall{};
	Wall.Type = EBodyType::Static;
	Wall.Position = FVector(0.0f, 0.0f, 100.0f);
	Wall.HalfExtents = FVector(50.0f, 50.0f, 100.0f);
	Physics.GetBodies().Add(Wall);

	UNavigationSystem Nav;
	Nav.SetCellSize(100.0f);
	Nav.BuildFromPhysScene(Physics, 0.0f, 400.0f);
	if (!TestTrue("Nav mesh built", Nav.HasNavMesh()))
	{
		return false;
	}

	FDebugDraw Draw;
	TestTrue("Starts empty", Draw.IsEmpty());
	Nav.AppendDebugDraw(Draw);
	TestFalse("Filled", Draw.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayCharacterResetJumpAndPerformMovementTest,
	"System.AIModule.Gameplay.CharacterResetJumpAndPerformMovement",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayCharacterResetJumpAndPerformMovementTest::RunTest(const FString& Parameters)
{
	// Reset puts the Character on the ground; Jump makes it fall upward, and movement input moves it along X.
	ACharacter Character;
	Character.Reset(FVector(0.0f, 0.0f, 0.0f), 45.0f);
	TestTrue("On ground after reset", Character.IsMovingOnGround());
	TestEqual("Yaw after reset", Character.GetActorYaw(), 45.0f, 1.0e-5f);

	FPhysScene Scene;
	Character.Jump();
	Character.PerformMovement(Scene, 1.0f / 60.0f);
	TestFalse("Left the ground", Character.IsMovingOnGround());
	TestTrue("Falling", Character.IsFalling());
	TestTrue("Moved up", Character.GetActorLocation().Z > 0.0f);

	Character.AddMovementInput(FVector(1.0f, 0.0f, 0.0f));
	const float StartX = Character.GetActorLocation().X;
	Character.PerformMovement(Scene, 1.0f / 60.0f);
	TestTrue("Moved along X", Character.GetActorLocation().X > StartX);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayActorSyncTransformToLevelWritesLinkedMeshTest,
	"System.AIModule.Gameplay.ActorSyncTransformToLevelWritesLinkedMesh",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayActorSyncTransformToLevelWritesLinkedMeshTest::RunTest(const FString& Parameters)
{
	// SyncTransformToLevel copies the Actor location and yaw into its linked Level mesh; the mesh shows converted
	// legacy content (facing +Y), so its yaw is the actor yaw plus LegacyContentYawDegrees.
	ULevel Level;
	UStaticMeshComponent Mesh{};
	Level.AddStaticMesh(MoveTemp(Mesh));

	UWorld World;
	ATestActor* Actor = World.SpawnActor<ATestActor>();
	Actor->SetLevelMeshIndex(0);
	Actor->SetActorLocationAndRotation(FVector(300.0f, 150.0f, -200.0f), 90.0f);
	Actor->SyncTransformToLevel(Level);

	const FTransform& Transform = Level.GetStaticMeshes()[0].Transform;
	TestEqual("Position X", Transform.GetLocation().X, 300.0f, 1.0e-3f);
	TestEqual("Position Y", Transform.GetLocation().Y, 150.0f, 1.0e-3f);
	TestEqual("Position Z", Transform.GetLocation().Z, -200.0f, 1.0e-3f);
	TestTrue("Yaw",
		Transform.GetRotation().Equals(FRotator(0.0f, 90.0f + LegacyContentYawDegrees, 0.0f).Quaternion(), 1.0e-6f));
	// The content's forward (+Y) now faces the actor's forward (yaw 90: +Y).
	TestTrue("Content faces the actor forward",
		Transform.GetRotation().RotateVector(FVector(0.0f, 1.0f, 0.0f)).Equals(FVector(0.0f, 1.0f, 0.0f), 1.0e-5f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayWorldTickGameplayFrameSyncsCharacterTest,
	"System.AIModule.Gameplay.WorldTickGameplayFrameSyncsCharacterToLevelMesh",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayWorldTickGameplayFrameSyncsCharacterTest::RunTest(const FString& Parameters)
{
	// A gameplay frame writes the Character pose into its linked Level mesh.
	ULevel Level;
	UStaticMeshComponent Mesh{};
	Level.AddStaticMesh(MoveTemp(Mesh));

	UWorld World;
	ACharacter* Character = World.SpawnActor<ACharacter>();
	Character->SetLevelMeshIndex(0);
	Character->Reset(FVector(100.0f, 200.0f, 0.0f), 45.0f);

	FWorldGameplayFrameParams Frame{};
	Frame.DeltaTime = 1.0f / 60.0f;
	Frame.Level = &Level;
	World.TickGameplayFrame(Frame);

	const FTransform& Transform = Level.GetStaticMeshes()[0].Transform;
	TestEqual("Position X", Transform.GetLocation().X, 100.0f, 1.0e-2f);
	TestEqual("Position Y", Transform.GetLocation().Y, 200.0f, 1.0e-2f);
	TestEqual("Yaw", Transform.Rotator().Yaw, 45.0f + LegacyContentYawDegrees, 1.0e-3f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayActorComponentRegisterAndSubobjectTickTest,
	"System.AIModule.Gameplay.ActorComponentRegisterComponentAndCreateDefaultSubobjectTick",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayActorComponentRegisterAndSubobjectTickTest::RunTest(const FString& Parameters)
{
	// A heap subobject is registered on its owner, begins play with it, ticks when enabled and stops once destroyed.
	struct UCountingComponent : UActorComponent
	{
		int32 Ticks = 0;
		int32 Begins = 0;
		void BeginPlay() override
		{
			++Begins;
		}
		void TickComponent(float) override
		{
			++Ticks;
		}
	};

	UWorld World;
	ATestActor* Actor = World.SpawnActor<ATestActor>();
	TestTrue("Root registered", Actor->GetComponents().Num() >= 1);

	UCountingComponent* Heap = Actor->CreateDefaultSubobject<UCountingComponent>();
	if (!TestNotNull("Subobject created", Heap))
	{
		return false;
	}
	TestTrue("Owned by actor", Heap->GetOwner() == Actor);
	TestTrue("Registered", Heap->IsRegistered());
	Heap->SetComponentTickEnabled(true);

	// BeginPlayComponents runs on spawn before Actor::BeginPlay.
	TestEqual("Began play once", Heap->Begins, 1);

	World.Tick(1.0f / 60.0f);
	TestEqual("Ticked once", Heap->Ticks, 1);

	Heap->DestroyComponent();
	TestFalse("Unregistered", Heap->IsRegistered());
	World.Tick(1.0f / 60.0f);
	TestEqual("No tick after unregister", Heap->Ticks, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplaySceneComponentAttachHierarchyTest,
	"System.AIModule.Gameplay.SceneComponentAttachHierarchyWorldTransform",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplaySceneComponentAttachHierarchyTest::RunTest(const FString& Parameters)
{
	// Attached components add their relative offsets to the Actor pose; cycles are refused and destroy detaches.
	UWorld World;
	ATestActor* Actor = World.SpawnActor<ATestActor>();
	Actor->SetActorLocationAndRotation(FVector(1000.0f, 0.0f, 0.0f), 0.0f);

	// The Relative* fields are world units (cm).
	USceneComponent Child;
	Child.SetOwner(Actor);
	Child.RelativeLocation = FVector(200.0f, 0.0f, 0.0f);
	TestTrue("Child attached", Child.AttachToComponent(&Actor->GetRootComponent()));
	TestTrue("Child parent is root", Child.GetAttachParent() == &Actor->GetRootComponent());
	TestEqual("Root has one child", Actor->GetRootComponent().GetAttachChildren().Num(), 1);

	const FVector Loc = Child.GetComponentLocation();
	TestEqual("Child world X", Loc.X, 1200.0f, 1.0e-2f);

	USceneComponent Grandchild;
	Grandchild.RelativeLocation = FVector(100.0f, 0.0f, 0.0f);
	TestTrue("Grandchild attached", Grandchild.AttachToComponent(&Child));
	TestEqual("Grandchild world X", Grandchild.GetComponentLocation().X, 1300.0f, 1.0e-2f);

	TestFalse("Cycle refused", Child.AttachToComponent(&Grandchild));
	Child.DestroyComponent();
	TestNull("Child detached", Child.GetAttachParent());
	TestEqual("Root has no children", Actor->GetRootComponent().GetAttachChildren().Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayCharacterMeshAttachesToRootTest,
	"System.AIModule.Gameplay.CharacterMeshAttachesToRootSceneComponent",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayCharacterMeshAttachesToRootTest::RunTest(const FString& Parameters)
{
	// The Character mesh is a registered child of the root and follows the Actor location plus its offset.
	ACharacter Character;
	TestTrue("Mesh parent is root", Character.GetMesh().GetAttachParent() == &Character.GetRootComponent());
	TestTrue("Mesh owner", Character.GetMesh().GetOwner() == &Character);
	TestTrue("Mesh registered", Character.GetMesh().IsRegistered());
	TestTrue("Root registered", Character.GetRootComponent().IsRegistered());
	Character.SetActorLocation(FVector(500.0f, 0.0f, 0.0f));
	// A relative offset of 100 cm, in the actor's space.
	Character.GetMesh().RelativeLocation = FVector(100.0f, 0.0f, 0.0f);
	TestEqual("Mesh world X", Character.GetMesh().GetComponentLocation().X, 600.0f, 1.0e-2f);

	// The mesh shows legacy content (facing +Y) with a relative yaw of -90: the content faces the actor's forward.
	Character.SetActorYaw(30.0f);
	const FVector ContentForward =
		Character.GetMesh().GetComponentTransform().GetRotation().RotateVector(FVector(0.0f, 1.0f, 0.0f));
	TestTrue("Content faces the actor forward", ContentForward.Equals(FRotator(0.0f, 30.0f, 0.0f).Vector(), 1.0e-5f));
	TestTrue("Offset turns with the actor",
		Character.GetMesh().GetComponentLocation().Equals(
			FVector(500.0f, 0.0f, 0.0f) + FRotator(0.0f, 30.0f, 0.0f).RotateVector(FVector(100.0f, 0.0f, 0.0f)),
			1.0e-2f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameplayPhysSceneReportsArcadeBackendByDefaultTest,
	"System.AIModule.Gameplay.PhysSceneReportsArcadeBackendByDefault",
	EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FGameplayPhysSceneReportsArcadeBackendByDefaultTest::RunTest(const FString& Parameters)
{
	// A default physics scene uses the Arcade backend and names it "Arcade".
	FPhysScene Scene;
	TestTrue("Arcade backend", Scene.GetBackend() == EPhysicsBackend::Arcade);
	TestEqual("Backend name", PhysicsBackendName(Scene.GetBackend()), "Arcade");
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
